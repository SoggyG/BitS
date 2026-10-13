#include "bencode.h"
#include <stdexcept>
#include <charconv>
BencodeValue decode_bencode(std::string_view& data);

static BencodeString decode_string(std::string_view& data) {
    size_t colon_pos = data.find(':');
    if (colon_pos == std::string_view::npos) {
        throw std::runtime_error("Invalid bencode string: missing colon");
    }

    size_t len = 0;
    auto [ptr, ec] = std::from_chars(data.data(), data.data() + colon_pos, len);
    if (ec != std::errc()) {
        throw std::runtime_error("Invalid bencode string length");
    }

    data.remove_prefix(colon_pos + 1); // Skip past the colon

    if (data.size() < len) {
        throw std::runtime_error("Invalid bencode string: truncated data");
    }

    BencodeString str(data.substr(0, len));
    data.remove_prefix(len); // Consume the string bytes
    return str;
}
static BencodeInt decode_integer(std::string_view& data) {
    data.remove_prefix(1); // Skip 'i'
    size_t e_pos = data.find('e');
    if (e_pos == std::string_view::npos) {
        throw std::runtime_error("Invalid bencode integer: missing 'e'");
    }

    BencodeInt val = 0;
    auto [ptr, ec] = std::from_chars(data.data(), data.data() + e_pos, val);
    if (ec != std::errc()) {
        throw std::runtime_error("Invalid bencode integer value");
    }

    data.remove_prefix(e_pos + 1); // Skip number and 'e'
    return val;
}

static BencodeList decode_list(std::string_view& data) {
    data.remove_prefix(1); // Skip 'l'
    BencodeList list;

    while (!data.empty() && data[0] != 'e') {
        list.push_back(decode_bencode(data));
    }

    if (data.empty()) {
        throw std::runtime_error("Invalid bencode list: missing 'e'");
    }

    data.remove_prefix(1); // Skip 'e'
    return list;
}

static BencodeDict decode_dict(std::string_view& data) {
    data.remove_prefix(1); // Skip 'd'
    BencodeDict dict;

    while (!data.empty() && data[0] != 'e') {
        BencodeString key = decode_string(data);
        BencodeValue val = decode_bencode(data);
        dict[key] = val;
    }

    if (data.empty()) {
        throw std::runtime_error("Invalid bencode dictionary: missing 'e'");
    }

    data.remove_prefix(1); // Skip 'e'
    return dict;
}

BencodeValue decode_bencode(std::string_view& data) {
    if (data.empty()) {
        throw std::runtime_error("Empty bencode data");
    }

    char prefix = data[0];
    if (prefix == 'i') {
        return decode_integer(data);
    } else if (prefix == 'l') {
        return decode_list(data);
    } else if (prefix == 'd') {
        return decode_dict(data);
    } else if (prefix >= '0' && prefix <= '9') {
        return decode_string(data);
    } else {
        throw std::runtime_error("Unknown bencode type prefix");
    }
}