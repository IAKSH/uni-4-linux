#pragma once

#include "hid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace uni::touchpad {

enum class TouchpadError {
    NotFound,
    OpenFailed,
    ReadFailed,
    WriteFailed,
};

std::string_view describe(TouchpadError error) noexcept;

constexpr TouchpadError from_hid(hid::HidError error) noexcept {
    switch (error) {
    case hid::HidError::OpenFailed:
        return TouchpadError::OpenFailed;
    case hid::HidError::ReadFailed:
        return TouchpadError::ReadFailed;
    case hid::HidError::WriteFailed:
        return TouchpadError::WriteFailed;
    }
    return TouchpadError::ReadFailed;
}

inline constexpr std::uint8_t kReportId = 7;
inline constexpr std::size_t kPayloadSize = 1;

bool is_touchpad(std::uint16_t vendor_id, std::uint16_t product_id,
                 std::wstring_view product, std::wstring_view manufacturer) noexcept;

bool is_touchpad(const hid::HidDeviceInfo &device) noexcept;

// Pure selection policy: best PixArt/Uniwill touchpad, or nullptr.
const hid::HidDeviceInfo *
select_touchpad(std::span<const hid::HidDeviceInfo> devices) noexcept;

// Protocol logic is independent of hidapi and can be driven by any HidBackend.
template <hid::HidBackend HidT>
std::expected<bool, TouchpadError> get_enabled(HidT &device) {
    std::array<std::uint8_t, kPayloadSize> payload{};
    auto result = device.get_feature(kReportId, payload);
    if (!result)
        return std::unexpected(from_hid(result.error()));
    if (*result < kPayloadSize)
        return std::unexpected(TouchpadError::ReadFailed);
    return (payload[0] & 0x01) != 0;
}

template <hid::HidBackend HidT>
std::expected<void, TouchpadError> set_enabled(HidT &device, bool enable) {
    const std::array<std::uint8_t, kPayloadSize> payload{
        static_cast<std::uint8_t>(enable ? 0x01 : 0x00),
    };
    auto result = device.set_feature(kReportId, payload);
    if (!result)
        return std::unexpected(from_hid(result.error()));
    return {};
}

template <hid::HidBackend HidT>
std::expected<bool, TouchpadError> toggle(HidT &device) {
    auto current = get_enabled(device);
    if (!current)
        return std::unexpected(current.error());

    const bool next = !*current;
    if (auto result = set_enabled(device, next); !result)
        return std::unexpected(result.error());
    return next;
}

// Convenience overloads that open the production hidapi backend.
std::expected<bool, TouchpadError> get_enabled();
std::expected<void, TouchpadError> set_enabled(bool enable);
std::expected<bool, TouchpadError> toggle();

} // namespace uni::touchpad
