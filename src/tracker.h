#pragma once
#include <string>
#include <vector>
#include "torrent.h"

struct Peer {
    std::string ip;
    uint16_t port;
};

struct TrackerResponse {
    int interval = 0;
    std::vector<Peer> peers;
};

// Contacts the tracker via HTTP and retrieves a list of active peers
TrackerResponse contact_tracker(const TorrentFile& torrent, const std::string& peer_id, uint16_t port = 6881);