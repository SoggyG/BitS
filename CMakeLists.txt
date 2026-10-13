cmake_minimum_required(VERSION 3.15)
project(BitTorrentClient VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(torrent_client
        src/main.cpp
        src/bencode.cpp
        src/bencode.h
        src/torrent.cpp
        src/torrent.h
        src/tracker.cpp
        src/tracker.h
)

target_link_libraries(torrent_client PRIVATE ws2_32)

target_link_libraries(torrent_client PRIVATE ws2_32 wininet)