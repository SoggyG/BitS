#include <iostream>
#include "bencode.h"

int main() {
    // Test a bencoded string and integer
    std::string raw_data = "4:spam";
    std::string_view view = raw_data;

    try {
        BencodeValue val = decode_bencode(view);
        if (std::holds_alternative<BencodeString>(val)) {
            std::cout << "Successfully decoded string: " << std::get<BencodeString>(val) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }

    return 0;
}