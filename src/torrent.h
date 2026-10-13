#pragma once

#include <string>
#include <vector>
#include "bencode.h"

struct TorrentFile {
    std::string announce;
    std::string name;
    size_t piece_length = 0;
    std::vector<std::string> piece_hashes; // 20-byte SHA-1 hashes split from 'pieces'
    size_t total_length = 0;
    std::string info_hash_hex;            // Hex representation for logging/display
    std::vector<unsigned char> info_hash_raw; // Raw 20-byte info hash for tracker requests
};

// Reads a .torrent file from disk, parses it, and populates TorrentFile
TorrentFile parse_torrent_file(const std::string& filepath);