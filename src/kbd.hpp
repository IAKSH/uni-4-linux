#pragma once

#include "ec.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>

namespace uni::kbd {

struct Rgb {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

struct KbdState {
    Rgb levels{};
    int brightness{};
    bool power{};
};

enum class KbdType {
    None,
    FixedColor,
    OneZoneRgb,
};

struct KbdInfo {
    std::uint8_t project_id{};
    std::uint8_t keyboard_id{};
    KbdType type{KbdType::None};
};

// The EC stores colour levels in 0..50; 0..255 input is rounded up to that grid.
constexpr std::uint8_t scale_rgb(std::uint8_t value) noexcept {
    const auto scaled = static_cast<unsigned>((value + 4u) / 5u);
    return static_cast<std::uint8_t>(scaled > kRgbMax ? kRgbMax : scaled);
}

template <EcBackend EcT>
std::expected<void, EcError> commit(EcT &ec) {
    return ec.update(reg(Reg::KbColorTrigger), kKbTriggerRgb, kKbTriggerRgb);
}

// Write already-scaled 0..50 levels, exit firmware rainbow, and commit.
template <EcBackend EcT>
std::expected<void, EcError> write_levels(EcT &ec, Rgb levels) {
    if (auto result = ec.write(reg(Reg::KbColorR), levels.r); !result)
        return result;
    if (auto result = ec.write(reg(Reg::KbColorG), levels.g); !result)
        return result;
    if (auto result = ec.write(reg(Reg::KbColorB), levels.b); !result)
        return result;

    return ec.update(reg(Reg::KbColorTrigger), kKbTriggerRainbow | kKbTriggerRgb,
                     kKbTriggerRgb);
}

template <EcBackend EcT>
std::expected<void, EcError> set_color(EcT &ec, Rgb rgb) {
    return write_levels(ec, Rgb{scale_rgb(rgb.r), scale_rgb(rgb.g), scale_rgb(rgb.b)});
}

// Brightness 0..4 lives in bits 5..7 of 0x078c; bit 4 applies it.
template <EcBackend EcT>
std::expected<void, EcError> set_brightness(EcT &ec, int brightness) {
    const auto level = std::clamp(brightness, 0, 4);
    const auto value = static_cast<std::uint8_t>((level << 5) | kKbBrightnessApply);
    return ec.update(reg(Reg::KbEnable), 0xf0, value);
}

template <EcBackend EcT>
std::expected<void, EcError> set_power(EcT &ec, bool on) {
    if (on) {
        if (auto result = ec.update(reg(Reg::KbEnable), kKbPowerOff, 0); !result)
            return result;
        if (auto result = ec.rewrite(reg(Reg::KbColorTrigger)); !result)
            return result;
        if (auto result = ec.rewrite(reg(Reg::KbPowerPrep)); !result)
            return result;

        auto red = ec.read(reg(Reg::KbColorR));
        if (!red)
            return std::unexpected(red.error());
        auto green = ec.read(reg(Reg::KbColorG));
        if (!green)
            return std::unexpected(green.error());
        auto blue = ec.read(reg(Reg::KbColorB));
        if (!blue)
            return std::unexpected(blue.error());

        if (auto result = ec.write(reg(Reg::KbColorR), *red); !result)
            return result;
        if (auto result = ec.write(reg(Reg::KbColorG), *green); !result)
            return result;
        if (auto result = ec.write(reg(Reg::KbColorB), *blue); !result)
            return result;
        return commit(ec);
    }

    if (auto result = ec.rewrite(reg(Reg::KbColorTrigger)); !result)
        return result;
    if (auto result = ec.rewrite(reg(Reg::KbPowerPrep)); !result)
        return result;
    return ec.update(reg(Reg::KbEnable), kKbPowerOff, kKbPowerOff);
}

template <EcBackend EcT>
std::expected<void, EcError> off(EcT &ec) {
    return set_power(ec, false);
}

template <EcBackend EcT>
std::expected<void, EcError> set_effect(EcT &ec, bool rainbow) {
    return ec.update(reg(Reg::KbColorTrigger), kKbTriggerRainbow,
                     rainbow ? kKbTriggerRainbow : std::uint8_t{0});
}

template <EcBackend EcT>
std::expected<KbdState, EcError> get_state(EcT &ec) {
    auto red = ec.read(reg(Reg::KbColorR));
    if (!red)
        return std::unexpected(red.error());
    auto green = ec.read(reg(Reg::KbColorG));
    if (!green)
        return std::unexpected(green.error());
    auto blue = ec.read(reg(Reg::KbColorB));
    if (!blue)
        return std::unexpected(blue.error());
    auto enable = ec.read(reg(Reg::KbEnable));
    if (!enable)
        return std::unexpected(enable.error());

    const auto brightness = std::min<int>(4, (*enable >> 5) & 0x07);
    return KbdState{
        Rgb{*red, *green, *blue},
        brightness,
        (*enable & kKbPowerOff) == 0,
    };
}

template <EcBackend EcT>
std::expected<KbdInfo, EcError> probe(EcT &ec) {
    auto project = ec.read(reg(Reg::ProjectId));
    if (!project)
        return std::unexpected(project.error());

    const auto keyboard = ec.read(reg(Reg::KeyboardId)).value_or(0);
    const auto support = ec.read(reg(Reg::KbSupport)).value_or(0);
    const auto type = ((support & 0x04) != 0 || keyboard != 0)
                          ? KbdType::OneZoneRgb
                          : KbdType::FixedColor;
    return KbdInfo{*project, keyboard, type};
}

} // namespace uni::kbd
