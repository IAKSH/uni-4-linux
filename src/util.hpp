#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

namespace uni {

template <typename... Args>
void println(std::format_string<Args...> fmt, Args &&...args) {
    std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

template <typename... Args>
void eprintln(std::format_string<Args...> fmt, Args &&...args) {
    std::cerr << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

void hexdump(std::span<const std::uint8_t> bytes, unsigned start_address);

} // namespace uni
