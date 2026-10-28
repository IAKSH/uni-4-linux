#include "commands.hpp"
#include "input.hpp"
#include "support.hpp"
#include "util.hpp"

#include <uni/kbd.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <span>

namespace uni::cli {
namespace {

template <typename EcT>
std::expected<void, EcError> dump_ec(EcT &ec, std::uint16_t addr,
                                    std::uint32_t length) {
    std::array<std::uint8_t, 256> buffer{};
    std::uint32_t position = 0;

    while (position < length) {
        const auto chunk = std::min<std::uint32_t>(
            length - position, static_cast<std::uint32_t>(buffer.size()));
        for (std::uint32_t i = 0; i < chunk; ++i) {
            const auto current = static_cast<std::uint16_t>(addr + position + i);
            auto value = ec.read(current);
            if (value) {
                buffer[i] = *value;
            } else {
                buffer[i] = 0xff;
                eprintln("read failed at 0x{:04x}", current);
            }
        }
        hexdump(std::span<const std::uint8_t>(buffer.data(), chunk),
                static_cast<unsigned>(addr + position));
        position += chunk;
    }
    return {};
}

} // namespace

int ec_read(std::uint16_t addr) {
    return with_ec([addr](auto &ec) -> std::expected<void, EcError> {
        auto value = ec.read(addr);
        if (!value)
            return std::unexpected(value.error());
        println("0x{:04x} = 0x{:02x}", addr, static_cast<unsigned>(*value));
        return {};
    });
}

int ec_write(std::uint16_t addr, std::uint8_t value) {
    return with_ec([addr, value](auto &ec) -> std::expected<void, EcError> {
        auto result = ec.write(addr, value);
        if (!result)
            return std::unexpected(result.error());
        println("0x{:04x} = 0x{:02x}", addr, static_cast<unsigned>(value));
        return {};
    });
}

int ec_dump(std::uint16_t addr, std::uint32_t length) {
    return with_ec([addr, length](auto &ec) -> std::expected<void, EcError> {
        return dump_ec(ec, addr, length);
    });
}

int kbd_test(kbd::Rgb rgb) {
    return with_ec([rgb](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        if (auto result = keyboard.set_color(rgb); !result)
            return std::unexpected(result.error());
        if (auto result = keyboard.set_brightness(4); !result)
            return std::unexpected(result.error());

        println("after write, 0x0760-0x0770:");
        if (auto result = dump_ec(ec, 0x0760, 16); !result)
            return result;
        println("after write, 0x1800-0x1810:");
        return dump_ec(ec, 0x1800, 16);
    });
}

int keymap(std::uint16_t scancode, std::uint16_t keycode) {
    auto result = input::remap_key(scancode, keycode);
    if (!result) {
        eprintln("keymap: {}", input::describe(result.error()));
        return 1;
    }
    return 0;
}

int monitor() {
    auto result = input::monitor();
    if (!result) {
        eprintln("monitor: {}", input::describe(result.error()));
        return 1;
    }
    return 0;
}

} // namespace uni::cli
