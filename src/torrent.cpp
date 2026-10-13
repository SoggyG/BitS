#include "torrent.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

// --- Lightweight SHA-1 Implementation ---
namespace {
    uint32_t rol(uint32_t value, size_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void sha1(const unsigned char* data, size_t len, unsigned char hash[20]) {
        uint32_t h0 = 0x67452301;
        uint32_t h1 = 0xEFCDAB89;
        uint32_t h2 = 0x98BADCFE;
        uint32_t h3 = 0x10325476;
        uint32_t h4 = 0xC3D2E1F0;

        // Pre-processing: padding
        size_t new_len = (((len + 8) / 64) + 1) * 64;
        std::vector<unsigned char> msg(new_len, 0);
        std::memcpy(msg.data(), data, len);
        msg[len] = 0x80;

        uint64_t bit_len = static_cast<uint64_t>(len) * 8;
        for (int i = 0; i < 8; ++i) {
            msg[new_len - 1 - i] = static_cast<unsigned char>(bit_len >> (i * 8));
        }

        // Process chunks
        for (size_t chunk = 0; chunk < new_len; chunk += 64) {
            uint32_t w[80];
            const unsigned char* c = &msg[chunk];
            for (int i = 0; i < 16; ++i) {
                w[i] = (c[i * 4] << 24) | (c[i * 4 + 1] << 16) | (c[i * 4 + 2] << 8) | c[i * 4 + 3];
            }
            for (int i = 16; i < 80; ++i) {
                w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            }

            uint32_t a = h0, b = h1, c_val = h2, d = h3, e = h4;

            for (int i = 0; i < 80; ++i) {
                uint32_t f, k;
                if (i < 20) {
                    f = (b & c_val) | (~b & d);
                    k = 0x5A827999;
                } else if (i < 40) {
                    f = b ^ c_val ^ d;
                    k = 0x6ED9EBA1;
                } else if (i < 60) {
                    f = (b & c_val) | (b & d) | (c_val & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c_val ^ d;
                    k = 0xCA62C1D6;
                }

                uint32_t temp = rol(a, 5) + f + e + k + w[i];
                e = d;
                d = c_val;
                c_val = rol(b, 30);
                b = a;
                a = temp;
            }

            h0 += a; h1 += b; h2 += c_val; h3 += d; h4 += e;
        }

        // Output raw hash
        auto put_big_endian = [](uint32_t val, unsigned char* buf) {
            buf[0] = (val >> 24) & 0xFF;
            buf[1] = (val >> 16) & 0xFF;
            buf[2] = (val >> 8) & 0xFF;
            buf[3] = val & 0xFF;
        };
        put_big_endian(h0, hash + 0);
        put_big_endian(h1, hash + 4);
        put_big_endian(h2, hash + 8);
        put_big_endian(h3, hash + 12);
        put_big_endian(h4, hash + 16);
    }
}

TorrentFile parse_torrent_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open torrent file: " + filepath);
    }

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    // 2. Parse bencoded root
    std::string_view sv = content;
    BencodeValue root = decode_bencode(sv);
    // ... rest of the function remains the same

    if (!std::holds_alternative<BencodeDict>(root)) {
        throw std::runtime_error("Invalid torrent file: Root is not a dictionary.");
    }
    const auto& dict = std::get<BencodeDict>(root);

    TorrentFile result;

    // 3. Extract announce URL
    if (dict.count("announce")) {
        if (const auto* s = std::get_if<std::string>(&dict.at("announce"))) {
            result.announce = *s;
        }
    }

    // 4. Extract info dictionary and compute raw info_hash
    if (!dict.count("info")) {
        throw std::runtime_error("Invalid torrent file: Missing 'info' dictionary.");
    }

    // Locate exact byte span of the 'info' dictionary in the raw file content
    // We search for "4:infod" or similar pattern to find the exact bencoded info boundaries.
    size_t info_key_pos = content.find("4:info");
    if (info_key_pos == std::string::npos) {
        throw std::runtime_error("Could not locate 'info' key in raw torrent data.");
    }
    size_t info_val_start = info_key_pos + 6; // skip "4:info"
    
    // Parse the info dictionary to find its exact ending position
    std::string_view info_sv = std::string_view(content).substr(info_val_start);
    size_t length_before = info_sv.size();
    BencodeValue info_parsed = decode_bencode(info_sv);
    size_t length_after = info_sv.size();
    size_t info_val_len = length_before - length_after;

    // Hash the raw bencoded bytes of the info dictionary
    result.info_hash_raw.resize(20);
    sha1(reinterpret_cast<const unsigned char*>(content.data() + info_val_start), info_val_len, result.info_hash_raw.data());

    // Convert raw hash to hex string for debugging
    std::ostringstream hex_oss;
    for (unsigned char b : result.info_hash_raw) {
        hex_oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    result.info_hash_hex = hex_oss.str();

    // 5. Parse fields inside the info dictionary
    const auto& info_dict = std::get<BencodeDict>(info_parsed);

    if (info_dict.count("name")) {
        if (const auto* s = std::get_if<std::string>(&info_dict.at("name"))) {
            result.name = *s;
        }
    }

    if (info_dict.count("piece length")) {
        if (const auto* i = std::get_if<int64_t>(&info_dict.at("piece length"))) {
            result.piece_length = static_cast<size_t>(*i);
        }
    }

    if (info_dict.count("pieces")) {
        if (const auto* s = std::get_if<std::string>(&info_dict.at("pieces"))) {
            // Pieces is a binary string of concatenated 20-byte SHA-1 hashes
            const std::string& pieces_blob = *s;
            for (size_t i = 0; i < pieces_blob.size(); i += 20) {
                result.piece_hashes.push_back(pieces_blob.substr(i, 20));
            }
        }
    }

    if (info_dict.count("length")) {
        if (const auto* i = std::get_if<int64_t>(&info_dict.at("length"))) {
            result.total_length = static_cast<size_t>(*i);
        }
    }

    return result;
}