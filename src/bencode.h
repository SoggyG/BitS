#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <cstdint>
#include <string_view>

struct BencodeValue;

using BencodeString = std::string;
using BencodeInt = int64_t;
using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;
struct BencodeValue : public std::variant<BencodeString, BencodeInt, BencodeList, BencodeDict> {
    using variant::variant;
};
BencodeValue decode_bencode(std::string_view& data);