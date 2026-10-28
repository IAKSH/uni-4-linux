#include "util.hpp"

void uni4linux::hexdump(std::span<const std::uint8_t> bytes, unsigned start_address) {
    for (std::size_t i = 0; i < bytes.size(); i += 16) {
        std::cout << std::format("{:04x}: ", start_address + static_cast<unsigned>(i));
        for (std::size_t j = 0; j < 16; ++j) {
            if (i + j < bytes.size())
                std::cout << std::format("{:02x} ", static_cast<unsigned>(bytes[i + j]));
            else
                std::cout << "   ";
        }
        std::cout << ' ';
        for (std::size_t j = 0; j < 16; ++j) {
            if (i + j < bytes.size()) {
                const auto c = bytes[i + j];
                std::cout << (c >= 0x20 && c < 0x7f ? static_cast<char>(c) : '.');
            } else {
                std::cout << ' ';
            }
        }
        std::cout << '\n';
    }
}
