#pragma once

#include <uni/hid.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <utility>

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

bool is_touchpad(std::uint16_t vendor_id, std::uint16_t product_id,
                 std::wstring_view product, std::wstring_view manufacturer) noexcept;

bool is_touchpad(const hid::HidDeviceInfo &device) noexcept;

// Pure selection policy: best PixArt/Uniwill touchpad, or nullptr.
const hid::HidDeviceInfo *
select_touchpad(std::span<const hid::HidDeviceInfo> devices) noexcept;

// A touchpad session owning its HID backend.  The protocol logic is
// independent of hidapi and can be driven by any HidBackend.
template <hid::HidBackend HidT>
class Touchpad {
public:
    static constexpr std::uint8_t report_id = 7;
    static constexpr std::size_t payload_size = 1;

    explicit Touchpad(HidT device) : device_(std::move(device)) {}

    [[nodiscard]] std::expected<bool, TouchpadError> is_enabled() {
        std::array<std::uint8_t, payload_size> payload{};
        auto result = device_.get_feature(report_id, payload);
        if (!result)
            return std::unexpected(from_hid(result.error()));
        if (*result < payload_size)
            return std::unexpected(TouchpadError::ReadFailed);
        return (payload[0] & 0x01) != 0;
    }

    [[nodiscard]] std::expected<void, TouchpadError> set_enabled(bool enable) {
        const std::array<std::uint8_t, payload_size> payload{
            static_cast<std::uint8_t>(enable ? 0x01 : 0x00),
        };
        auto result = device_.set_feature(report_id, payload);
        if (!result)
            return std::unexpected(from_hid(result.error()));
        return {};
    }

    [[nodiscard]] std::expected<bool, TouchpadError> toggle() {
        auto current = is_enabled();
        if (!current)
            return std::unexpected(current.error());

        const bool next = !*current;
        if (auto result = set_enabled(next); !result)
            return std::unexpected(result.error());
        return next;
    }

    [[nodiscard]] HidT &device() noexcept { return device_; }
    [[nodiscard]] const HidT &device() const noexcept { return device_; }

private:
    HidT device_;
};

using HidTouchpad = Touchpad<hid::HidApi>;

// Open the production hidapi-backed touchpad session.
std::expected<HidTouchpad, TouchpadError> open();

} // namespace uni::touchpad
