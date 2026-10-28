#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace uni4linux::touchpad {

enum class TouchpadError {
    NotFound,
    OpenFailed,
    ReadFailed,
    WriteFailed,
};

std::string_view describe(TouchpadError error) noexcept;

// Selection predicate kept free of hidapi types so it can be unit-tested.
bool is_touchpad(std::uint16_t vendor_id, std::uint16_t product_id,
                 std::wstring_view product, std::wstring_view manufacturer) noexcept;

std::expected<bool, TouchpadError> get_enabled();
std::expected<void, TouchpadError> set_enabled(bool enable);
std::expected<bool, TouchpadError> toggle();

} // namespace uni4linux::touchpad
