#include <iostream>
#include "torrent.h"
#include "tracker.h"
// Logger feature branch update
int main() {
    try {
        std::string torrent_path = "C:/Users/bogia/Desktop/ubuntu-26.04.1-desktop-amd64.iso.torrent"; // Your test .torrent file
        TorrentFile tf = parse_torrent_file(torrent_path);

        std::cout << "Parsed Torrent: " << tf.name << "\n";
        std::cout << "Contacting tracker at: " << tf.announce << " ...\n";

        // BitTorrent peer IDs usually start with a client prefix (e.g. -PC0001-) followed by random bytes
        std::string peer_id = "-PC0001-1234567890ab";

        TrackerResponse tr = contact_tracker(tf, peer_id);

        std::cout << "Successfully connected to tracker!???\n";
        std::cout << "Refresh Interval: " << tr.interval << " seconds\n";
        std::cout << "Peers Discovered: " << tr.peers.size() << "\n";

        for (size_t i = 0; i < std::min(size_t(5), tr.peers.size()); ++i) {
            std::cout << "  - Peer " << (i + 1) << ": " << tr.peers[i].ip << ":" << tr.peers[i].port << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}