#include "ec.hpp"
#include "fan.hpp"
#include "input.hpp"
#include "kbd.hpp"
#include "touchpad.hpp"
#include "util.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#ifndef UNI4_VERSION
#define UNI4_VERSION "0.1.0"
#endif

namespace kbd = uni4linux::kbd;
namespace fan = uni4linux::fan;
namespace tp = uni4linux::touchpad;
namespace input = uni4linux::input;

using uni4linux::AcpiEc;
using uni4linux::Ec;
using uni4linux::EcError;

template <typename Fn>
int with_ec(Fn &&fn) {
    auto backend = AcpiEc::open();
    if (!backend) {
        uni4linux::eprintln("EC: {}", uni4linux::describe(backend.error()));
        return 1;
    }

    Ec ec{std::move(*backend)};
    auto result = fn(ec);
    if (!result) {
        uni4linux::eprintln("EC: {}", uni4linux::describe(result.error()));
        return 1;
    }
    return 0;
}

template <typename T>
std::optional<T> parse_number(std::string_view text) {
    if (text.empty())
        return std::nullopt;

    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty())
        return std::nullopt;

    T value{};
    const auto *begin = text.data();
    const auto *end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, value, base);
    if (error != std::errc{} || ptr != end)
        return std::nullopt;
    return value;
}

std::optional<std::uint16_t> parse_scancode(std::string_view text) {
    if (text.starts_with("0x") || text.starts_with("0X"))
        text.remove_prefix(2);
    if (text.empty())
        return std::nullopt;

    std::uint16_t value{};
    const auto *begin = text.data();
    const auto *end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, value, 16);
    if (error != std::errc{} || ptr != end)
        return std::nullopt;
    return value;
}

std::string_view type_name(kbd::KbdType type) {
    switch (type) {
    case kbd::KbdType::OneZoneRgb:
        return "1-zone RGB";
    case kbd::KbdType::FixedColor:
        return "fixed colour";
    case kbd::KbdType::None:
        return "unknown/none";
    }
    return "unknown/none";
}

template <typename EcT>
std::expected<void, EcError> dump_ec(EcT &ec, std::uint16_t addr, std::uint32_t length) {
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
                uni4linux::eprintln("read failed at 0x{:04x}", current);
            }
        }
        uni4linux::hexdump(std::span<const std::uint8_t>(buffer.data(), chunk),
                           static_cast<unsigned>(addr + position));
        position += chunk;
    }
    return {};
}

int cmd_probe() {
    return with_ec([](auto &ec) -> std::expected<void, EcError> {
        auto info = kbd::probe(ec);
        if (!info)
            return std::unexpected(info.error());

        uni4linux::println("ec: project_id 0x{:02x} keyboard_id 0x{:02x}",
                           static_cast<unsigned>(info->project_id),
                           static_cast<unsigned>(info->keyboard_id));
        uni4linux::println("backlight type: {}", type_name(info->type));

        auto state = kbd::get_state(ec);
        if (!state)
            return std::unexpected(state.error());
        uni4linux::println("backlight: R={} G={} B={} brightness={} power={}",
                           static_cast<unsigned>(state->levels.r),
                           static_cast<unsigned>(state->levels.g),
                           static_cast<unsigned>(state->levels.b), state->brightness,
                           state->power ? "on" : "off");

        auto touchpad = tp::get_enabled();
        if (touchpad)
            uni4linux::println("touchpad: {}", *touchpad ? "enabled" : "disabled");
        else
            uni4linux::println("touchpad: {}", tp::describe(touchpad.error()));
        return {};
    });
}

int cmd_ec_read(std::uint16_t addr) {
    return with_ec([addr](auto &ec) -> std::expected<void, EcError> {
        auto value = ec.read(addr);
        if (!value)
            return std::unexpected(value.error());
        uni4linux::println("0x{:04x} = 0x{:02x}", addr,
                           static_cast<unsigned>(*value));
        return {};
    });
}

int cmd_ec_write(std::uint16_t addr, std::uint8_t value) {
    return with_ec([addr, value](auto &ec) -> std::expected<void, EcError> {
        auto result = ec.write(addr, value);
        if (!result)
            return std::unexpected(result.error());
        uni4linux::println("0x{:04x} = 0x{:02x}", addr,
                           static_cast<unsigned>(value));
        return {};
    });
}

int cmd_ec_dump(std::uint16_t addr, std::uint32_t length) {
    return with_ec([addr, length](auto &ec) -> std::expected<void, EcError> {
        return dump_ec(ec, addr, length);
    });
}

int cmd_kbd_color(kbd::Rgb rgb) {
    return with_ec([rgb](auto &ec) -> std::expected<void, EcError> {
        return kbd::set_color(ec, rgb);
    });
}

int cmd_kbd_brightness(int brightness) {
    return with_ec([brightness](auto &ec) -> std::expected<void, EcError> {
        return kbd::set_brightness(ec, brightness);
    });
}

int cmd_kbd_power(bool on) {
    return with_ec([on](auto &ec) -> std::expected<void, EcError> {
        return kbd::set_power(ec, on);
    });
}

int cmd_kbd_effect(bool rainbow) {
    return with_ec([rainbow](auto &ec) -> std::expected<void, EcError> {
        return kbd::set_effect(ec, rainbow);
    });
}

int cmd_kbd_off() {
    return with_ec([](auto &ec) -> std::expected<void, EcError> {
        return kbd::off(ec);
    });
}

int cmd_kbd_breathe(kbd::Rgb rgb, int period_ms) {
    return with_ec([rgb, period_ms](auto &ec) -> std::expected<void, EcError> {
        const kbd::Rgb levels{
            kbd::scale_rgb(rgb.r),
            kbd::scale_rgb(rgb.g),
            kbd::scale_rgb(rgb.b),
        };
        const auto frame = std::chrono::milliseconds(std::max(1, period_ms / 100));
        uni4linux::eprintln(
            "breathing colour {},{},{} period {}ms, Ctrl-C to stop",
            static_cast<unsigned>(rgb.r), static_cast<unsigned>(rgb.g),
            static_cast<unsigned>(rgb.b), period_ms);

        int level = 0;
        int direction = 1;
        for (;;) {
            const kbd::Rgb current{
                static_cast<std::uint8_t>(levels.r * level / uni4linux::kRgbMax),
                static_cast<std::uint8_t>(levels.g * level / uni4linux::kRgbMax),
                static_cast<std::uint8_t>(levels.b * level / uni4linux::kRgbMax),
            };
            auto result = kbd::write_levels(ec, current);
            if (!result)
                return std::unexpected(result.error());

            std::this_thread::sleep_for(frame);
            level += direction;
            if (level >= uni4linux::kRgbMax) {
                level = uni4linux::kRgbMax;
                direction = -1;
            } else if (level <= 0) {
                level = 0;
                direction = 1;
            }
        }
    });
}

int cmd_kbd_test(kbd::Rgb rgb) {
    return with_ec([rgb](auto &ec) -> std::expected<void, EcError> {
        if (auto result = kbd::set_color(ec, rgb); !result)
            return std::unexpected(result.error());
        if (auto result = kbd::set_brightness(ec, 4); !result)
            return std::unexpected(result.error());

        uni4linux::println("after write, 0x0760-0x0770:");
        if (auto result = dump_ec(ec, 0x0760, 16); !result)
            return result;
        uni4linux::println("after write, 0x1800-0x1810:");
        return dump_ec(ec, 0x1800, 16);
    });
}

int cmd_tp_get() {
    auto state = tp::get_enabled();
    if (!state) {
        uni4linux::eprintln("touchpad: {}", tp::describe(state.error()));
        return 1;
    }
    uni4linux::println("{}", *state ? 1 : 0);
    return 0;
}

int cmd_tp_set(bool enable) {
    auto result = tp::set_enabled(enable);
    if (!result) {
        uni4linux::eprintln("touchpad: {}", tp::describe(result.error()));
        return 1;
    }
    uni4linux::println("touchpad {}", enable ? "enabled" : "disabled");
    return 0;
}

int cmd_tp_toggle() {
    auto state = tp::toggle();
    if (!state) {
        uni4linux::eprintln("touchpad: {}", tp::describe(state.error()));
        return 1;
    }
    uni4linux::println("touchpad {}", *state ? "enabled" : "disabled");
    return 0;
}

int cmd_tp_watch() {
    auto result = input::watch_touchpad();
    if (!result) {
        uni4linux::eprintln("tp watch: {}", input::describe(result.error()));
        return 1;
    }
    return 0;
}

int cmd_fan_get() {
    return with_ec([](auto &ec) -> std::expected<void, EcError> {
        auto state = fan::get(ec);
        if (!state)
            return std::unexpected(state.error());

        uni4linux::println("mode: 0x{:02x}{}{}", static_cast<unsigned>(state->mode),
                           (state->mode & uni4linux::kFanBoost) ? " (boost)" : "",
                           (state->mode & uni4linux::kFanManual) ? " (manual)" : "");
        uni4linux::println("duty: left {}% right {}%", state->left_duty / 2,
                           state->right_duty / 2);
        uni4linux::println("rpm:  left {} right {}", state->left_rpm,
                           state->right_rpm);
        return {};
    });
}

int cmd_fan_boost(bool on) {
    return with_ec([on](auto &ec) -> std::expected<void, EcError> {
        if (auto result = fan::set_boost(ec, on); !result)
            return std::unexpected(result.error());
        uni4linux::println("fan boost {}", on ? "on" : "off");
        return {};
    });
}

int cmd_keymap(std::uint16_t scancode, std::uint16_t keycode) {
    auto result = input::remap_key(scancode, keycode);
    if (!result) {
        uni4linux::eprintln("keymap: {}", input::describe(result.error()));
        return 1;
    }
    return 0;
}

int cmd_monitor() {
    auto result = input::monitor();
    if (!result) {
        uni4linux::eprintln("monitor: {}", input::describe(result.error()));
        return 1;
    }
    return 0;
}

void add_rgb_options(CLI::App &command, int &red, int &green, int &blue) {
    command.add_option("r", red, "red 0-255")->required()->check(CLI::Range(0, 255));
    command.add_option("g", green, "green 0-255")->required()->check(CLI::Range(0, 255));
    command.add_option("b", blue, "blue 0-255")->required()->check(CLI::Range(0, 255));
}

void add_on_off_option(CLI::App &command, std::string &state) {
    command.add_option("state", state, "on or off")
        ->required()
        ->check(CLI::IsMember({"on", "off"}));
}

int main(int argc, char **argv) {
    CLI::App app{"Uniwill EC control for the MECHREVO Jiguang Pro (GM5AG0O)"};
    app.set_version_flag("-V,--version", UNI4_VERSION);
    app.require_subcommand(1);

    auto *probe = app.add_subcommand("probe", "Print EC/backlight/touchpad state");

    auto *ec = app.add_subcommand("ec", "Raw EC register access");
    ec->require_subcommand(1);

    std::string read_addr;
    auto *ec_read = ec->add_subcommand("read", "Read one EC byte");
    ec_read->add_option("addr", read_addr, "16-bit address (e.g. 0x0769)")->required();

    std::string write_addr;
    std::string write_value;
    auto *ec_write = ec->add_subcommand("write", "Write one EC byte");
    ec_write->add_option("addr", write_addr, "16-bit address")->required();
    ec_write->add_option("value", write_value, "byte value (0-255 or 0x00-0xff)")
        ->required();

    std::string dump_addr;
    std::string dump_length;
    auto *ec_dump = ec->add_subcommand("dump", "Dump EC bytes");
    ec_dump->add_option("addr", dump_addr, "start address")->required();
    ec_dump->add_option("len", dump_length, "number of bytes")->required();

    auto *kbd_cmd = app.add_subcommand("kbd", "Keyboard backlight");
    kbd_cmd->require_subcommand(1);

    int rgb_r = 0;
    int rgb_g = 0;
    int rgb_b = 0;

    auto *kbd_color = kbd_cmd->add_subcommand("color", "Set static colour (0-255 each)");
    add_rgb_options(*kbd_color, rgb_r, rgb_g, rgb_b);

    int brightness = 0;
    auto *kbd_brightness = kbd_cmd->add_subcommand("brightness", "Set brightness 0-4");
    kbd_brightness->add_option("level", brightness, "0 (min) .. 4 (max)")
        ->required()
        ->check(CLI::Range(0, 4));

    std::string power_state;
    auto *kbd_power = kbd_cmd->add_subcommand("power", "Keyboard backlight master power");
    add_on_off_option(*kbd_power, power_state);

    std::string effect_state;
    auto *kbd_effect = kbd_cmd->add_subcommand("effect", "Firmware light effect");
    kbd_effect->add_option("effect", effect_state, "rainbow or static")
        ->required()
        ->check(CLI::IsMember({"rainbow", "static"}));

    auto *kbd_off = kbd_cmd->add_subcommand("off", "Turn the backlight off");

    int breathe_period = 3000;
    auto *kbd_breathe = kbd_cmd->add_subcommand("breathe", "Software breathing effect");
    add_rgb_options(*kbd_breathe, rgb_r, rgb_g, rgb_b);
    kbd_breathe->add_option("period", breathe_period, "cycle in milliseconds")
        ->check(CLI::Range(1, 600000));

    auto *kbd_test = kbd_cmd->add_subcommand("test", "Write a colour and dump registers");
    add_rgb_options(*kbd_test, rgb_r, rgb_g, rgb_b);

    auto *tp_cmd = app.add_subcommand("tp", "Touchpad switch and LED");
    tp_cmd->require_subcommand(1);
    auto *tp_get = tp_cmd->add_subcommand("get", "Print touchpad state (1 = enabled)");
    auto *tp_on = tp_cmd->add_subcommand("on", "Enable the touchpad");
    auto *tp_off = tp_cmd->add_subcommand("off", "Disable the touchpad");
    auto *tp_toggle = tp_cmd->add_subcommand("toggle", "Toggle the touchpad");
    auto *tp_watch = tp_cmd->add_subcommand("watch", "Toggle on Fn+F5 or KEY_F21");

    auto *fan_cmd = app.add_subcommand("fan", "Fan telemetry and boost");
    fan_cmd->require_subcommand(1);
    auto *fan_get = fan_cmd->add_subcommand("get", "Read fan mode/duty/RPM");
    std::string boost_state;
    auto *fan_boost = fan_cmd->add_subcommand("boost", "Fan boost (full speed)");
    add_on_off_option(*fan_boost, boost_state);

    std::string keymap_scancode;
    std::string keymap_keycode;
    auto *keymap = app.add_subcommand("keymap", "Map a scancode on the AT keyboard");
    keymap->add_option("scancode", keymap_scancode, "hex scancode (e.g. f8)")
        ->required();
    keymap->add_option("keycode", keymap_keycode, "evdev keycode (e.g. 350)")
        ->required();

    auto *monitor = app.add_subcommand("monitor", "Print input key events");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &error) {
        return app.exit(error);
    }

    if (probe->count() > 0)
        return cmd_probe();

    if (ec_read->count() > 0) {
        const auto addr = parse_number<std::uint16_t>(read_addr);
        if (!addr) {
            uni4linux::eprintln("invalid address: {}", read_addr);
            return 2;
        }
        return cmd_ec_read(*addr);
    }

    if (ec_write->count() > 0) {
        const auto addr = parse_number<std::uint16_t>(write_addr);
        const auto value = parse_number<std::uint8_t>(write_value);
        if (!addr || !value) {
            uni4linux::eprintln("invalid address or value");
            return 2;
        }
        return cmd_ec_write(*addr, *value);
    }

    if (ec_dump->count() > 0) {
        const auto addr = parse_number<std::uint16_t>(dump_addr);
        const auto length = parse_number<std::uint32_t>(dump_length);
        if (!addr || !length || *length == 0 ||
            static_cast<std::uint32_t>(*addr) + *length > 0x10000u) {
            uni4linux::eprintln("invalid address or length");
            return 2;
        }
        return cmd_ec_dump(*addr, *length);
    }

    if (kbd_color->count() > 0)
        return cmd_kbd_color(kbd::Rgb{
            static_cast<std::uint8_t>(rgb_r),
            static_cast<std::uint8_t>(rgb_g),
            static_cast<std::uint8_t>(rgb_b),
        });

    if (kbd_brightness->count() > 0)
        return cmd_kbd_brightness(brightness);

    if (kbd_power->count() > 0)
        return cmd_kbd_power(power_state == "on");

    if (kbd_effect->count() > 0)
        return cmd_kbd_effect(effect_state == "rainbow");

    if (kbd_off->count() > 0)
        return cmd_kbd_off();

    if (kbd_breathe->count() > 0)
        return cmd_kbd_breathe(kbd::Rgb{
                                   static_cast<std::uint8_t>(rgb_r),
                                   static_cast<std::uint8_t>(rgb_g),
                                   static_cast<std::uint8_t>(rgb_b),
                               },
                               breathe_period);

    if (kbd_test->count() > 0)
        return cmd_kbd_test(kbd::Rgb{
            static_cast<std::uint8_t>(rgb_r),
            static_cast<std::uint8_t>(rgb_g),
            static_cast<std::uint8_t>(rgb_b),
        });

    if (tp_get->count() > 0)
        return cmd_tp_get();
    if (tp_on->count() > 0)
        return cmd_tp_set(true);
    if (tp_off->count() > 0)
        return cmd_tp_set(false);
    if (tp_toggle->count() > 0)
        return cmd_tp_toggle();
    if (tp_watch->count() > 0)
        return cmd_tp_watch();

    if (fan_get->count() > 0)
        return cmd_fan_get();
    if (fan_boost->count() > 0)
        return cmd_fan_boost(boost_state == "on");

    if (keymap->count() > 0) {
        const auto scancode = parse_scancode(keymap_scancode);
        const auto keycode = parse_number<std::uint16_t>(keymap_keycode);
        if (!scancode || !keycode) {
            uni4linux::eprintln("invalid scancode or keycode");
            return 2;
        }
        return cmd_keymap(*scancode, *keycode);
    }

    if (monitor->count() > 0)
        return cmd_monitor();

    std::cerr << app.help();
    return 1;
}
