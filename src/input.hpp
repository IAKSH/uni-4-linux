#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace uni4linux::input {

enum class InputError {
    NoDevices,
    OpenFailed,
    KeyboardNotFound,
    KeymapFailed,
};

std::string_view describe(InputError error) noexcept;

// Toggle the touchpad on Fn+F5 (Meta+Ctrl+F24) or a mapped KEY_F21.
std::expected<void, InputError> watch_touchpad();

// Print EV_KEY events from every readable /dev/input/event* device.
std::expected<void, InputError> monitor();

// Map a scancode on the AT keyboard (EVIOCSKEYCODE_V2, legacy fallback).
std::expected<void, InputError> remap_key(std::uint16_t scancode, std::uint16_t keycode);

} // namespace uni4linux::input
