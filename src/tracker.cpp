#include "tracker.h"
#include "bencode.h"
#include <windows.h>
#include <wininet.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

#pragma comment(lib, "wininet.lib")

namespace {
    // Helper to URL-encode binary data for HTTP query parameters
    std::string url_encode(const std::vector<unsigned char>& data) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        for (unsigned char c : data) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << '%' << std::setw(2) << static_cast<int>(c);
            }
        }
        return escaped.str();
    }
}

TrackerResponse contact_tracker(const TorrentFile& torrent, const std::string& peer_id, uint16_t client_port) {
    // 1. Build the full URL with query parameters
    std::string encoded_info_hash = url_encode(torrent.info_hash_raw);

    std::ostringstream query_oss;
    query_oss << torrent.announce
              << "?info_hash=" << encoded_info_hash
              << "&peer_id=" << peer_id
              << "&port=" << client_port
              << "&uploaded=0"
              << "&downloaded=0"
              << "&left=" << torrent.total_length
              << "&compact=1";
    std::string full_url = query_oss.str();

    // 2. Initialize WinINet session (supports HTTP and HTTPS natively)
    HINTERNET hInternet = InternetOpenA("BitTorrentClient/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        throw std::runtime_error("InternetOpen failed.");
    }

    // 3. Open the URL (handles secure SSL/TLS negotiation automatically)
    HINTERNET hFile = InternetOpenUrlA(
        hInternet,
        full_url.c_str(),
        NULL,
        0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_SECURE,
        0
    );

    if (!hFile) {
        InternetCloseHandle(hInternet);
        throw std::runtime_error("Failed to connect to tracker URL: " + full_url);
    }

    // 4. Read the response from the tracker
    std::vector<char> response_buffer;
    char buffer[4096];
    DWORD bytes_read = 0;
    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        response_buffer.insert(response_buffer.end(), buffer, buffer + bytes_read);
    }

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    std::string_view bencode_body(response_buffer.data(), response_buffer.size());

    // 5. Decode bencoded tracker response
    BencodeValue decoded = decode_bencode(bencode_body);
    if (!std::holds_alternative<BencodeDict>(decoded)) {
        throw std::runtime_error("Tracker response is not a valid bencoded dictionary.");
    }
    const auto& dict = std::get<BencodeDict>(decoded);

    TrackerResponse tracker_res;

    // Check if tracker returned an error message
    if (dict.count("failure reason")) {
        if (const auto* s = std::get_if<std::string>(&dict.at("failure reason"))) {
            throw std::runtime_error("Tracker error: " + *s);
        }
    }

    if (dict.count("interval")) {
        if (const auto* i = std::get_if<int64_t>(&dict.at("interval"))) {
            tracker_res.interval = static_cast<int>(*i);
        }
    }

    // 6. Parse compact peers (6 bytes per peer: 4 bytes IP, 2 bytes port)
    if (dict.count("peers")) {
        if (const auto* s = std::get_if<std::string>(&dict.at("peers"))) {
            const std::string& peers_blob = *s;
            for (size_t i = 0; i + 5 < peers_blob.size(); i += 6) {
                Peer peer;
                std::ostringstream ip_oss;
                ip_oss << static_cast<int>(static_cast<unsigned char>(peers_blob[i])) << "."
                       << static_cast<int>(static_cast<unsigned char>(peers_blob[i + 1])) << "."
                       << static_cast<int>(static_cast<unsigned char>(peers_blob[i + 2])) << "."
                       << static_cast<int>(static_cast<unsigned char>(peers_blob[i + 3]));
                peer.ip = ip_oss.str();
                peer.port = (static_cast<unsigned char>(peers_blob[i + 4]) << 8) |
                             static_cast<unsigned char>(peers_blob[i + 5]);
                tracker_res.peers.push_back(peer);
            }
        }
    }

    return tracker_res;
}