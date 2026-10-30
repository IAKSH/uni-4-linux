#pragma once

#include <uni/ec.hpp>

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
    bool rainbow{};
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

// Inverse of scale_rgb() for an EC level: the result maps back to the same
// level.  Level 50 expands to 255 so the UI can still reach full brightness.
constexpr std::uint8_t expand_rgb(std::uint8_t level) noexcept {
    return level >= kRgbMax ? 255
                            : static_cast<std::uint8_t>(level * 5u);
}

template <EcBackend EcT>
class Keyboard {
public:
    explicit Keyboard(EcT &ec) : ec_(ec) {}

    [[nodiscard]] std::expected<void, EcError> set_color(Rgb rgb) {
        return set_levels(Rgb{scale_rgb(rgb.r), scale_rgb(rgb.g), scale_rgb(rgb.b)});
    }

    // Write already-scaled 0..50 levels, exit firmware rainbow, and commit.
    [[nodiscard]] std::expected<void, EcError> set_levels(Rgb levels) {
        if (auto result = ec_.write(reg(Reg::KbColorR), levels.r); !result)
            return result;
        if (auto result = ec_.write(reg(Reg::KbColorG), levels.g); !result)
            return result;
        if (auto result = ec_.write(reg(Reg::KbColorB), levels.b); !result)
            return result;

        return ec_.update(reg(Reg::KbColorTrigger), kKbTriggerRainbow | kKbTriggerRgb,
                          kKbTriggerRgb);
    }

    // Brightness 0..4 lives in bits 5..7 of 0x078c; bit 4 applies it.
    [[nodiscard]] std::expected<void, EcError> set_brightness(int brightness) {
        const auto level = std::clamp(brightness, 0, 4);
        const auto value = static_cast<std::uint8_t>((level << 5) | kKbBrightnessApply);
        return ec_.update(reg(Reg::KbEnable), 0xf0, value);
    }

    [[nodiscard]] std::expected<void, EcError> set_power(bool on) {
        if (on) {
            if (auto result = ec_.update(reg(Reg::KbEnable), kKbPowerOff, 0); !result)
                return result;
            if (auto result = ec_.rewrite(reg(Reg::KbColorTrigger)); !result)
                return result;
            if (auto result = ec_.rewrite(reg(Reg::KbPowerPrep)); !result)
                return result;

            auto red = ec_.read(reg(Reg::KbColorR));
            if (!red)
                return std::unexpected(red.error());
            auto green = ec_.read(reg(Reg::KbColorG));
            if (!green)
                return std::unexpected(green.error());
            auto blue = ec_.read(reg(Reg::KbColorB));
            if (!blue)
                return std::unexpected(blue.error());

            if (auto result = ec_.write(reg(Reg::KbColorR), *red); !result)
                return result;
            if (auto result = ec_.write(reg(Reg::KbColorG), *green); !result)
                return result;
            if (auto result = ec_.write(reg(Reg::KbColorB), *blue); !result)
                return result;
            return commit();
        }

        if (auto result = ec_.rewrite(reg(Reg::KbColorTrigger)); !result)
            return result;
        if (auto result = ec_.rewrite(reg(Reg::KbPowerPrep)); !result)
            return result;
        return ec_.update(reg(Reg::KbEnable), kKbPowerOff, kKbPowerOff);
    }

    [[nodiscard]] std::expected<void, EcError> off() {
        return set_power(false);
    }

    [[nodiscard]] std::expected<void, EcError> set_effect(bool rainbow) {
        return ec_.update(reg(Reg::KbColorTrigger), kKbTriggerRainbow,
                          rainbow ? kKbTriggerRainbow : std::uint8_t{0});
    }

    [[nodiscard]] std::expected<KbdState, EcError> state() {
        auto red = ec_.read(reg(Reg::KbColorR));
        if (!red)
            return std::unexpected(red.error());
        auto green = ec_.read(reg(Reg::KbColorG));
        if (!green)
            return std::unexpected(green.error());
        auto blue = ec_.read(reg(Reg::KbColorB));
        if (!blue)
            return std::unexpected(blue.error());
        auto enable = ec_.read(reg(Reg::KbEnable));
        if (!enable)
            return std::unexpected(enable.error());

        const auto trigger = ec_.read(reg(Reg::KbColorTrigger)).value_or(0);
        const auto brightness = std::min<int>(4, (*enable >> 5) & 0x07);
        return KbdState{
            Rgb{*red, *green, *blue},
            brightness,
            (*enable & kKbPowerOff) == 0,
            (trigger & kKbTriggerRainbow) != 0,
        };
    }

    [[nodiscard]] std::expected<KbdInfo, EcError> probe() {
        auto project = ec_.read(reg(Reg::ProjectId));
        if (!project)
            return std::unexpected(project.error());

        const auto keyboard = ec_.read(reg(Reg::KeyboardId)).value_or(0);
        const auto support = ec_.read(reg(Reg::KbSupport)).value_or(0);
        const auto type = ((support & 0x04) != 0 || keyboard != 0)
                              ? KbdType::OneZoneRgb
                              : KbdType::FixedColor;
        return KbdInfo{*project, keyboard, type};
    }

private:
    [[nodiscard]] std::expected<void, EcError> commit() {
        return ec_.update(reg(Reg::KbColorTrigger), kKbTriggerRgb, kKbTriggerRgb);
    }

    EcT &ec_;
};

} // namespace uni::kbd
