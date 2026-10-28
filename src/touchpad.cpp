#include "touchpad.hpp"

#include <hidapi/hidapi.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace {

constexpr unsigned char kReportId = 7;
constexpr std::size_t kReportBufferSize = 2;
constexpr std::uint16_t kPixArtVendor = 0x093a;
constexpr std::uint16_t kTouchpadProduct = 0x0255;

struct HidDeleter {
    void operator()(hid_device *device) const noexcept {
        if (device)
            hid_close(device);
    }
};

using HidHandle = std::unique_ptr<hid_device, HidDeleter>;

int selection_score(const hid_device_info &info) noexcept {
    int score = 0;
    if (info.vendor_id == kPixArtVendor)
        score += 4;
    if (info.product_id == kTouchpadProduct)
        score += 2;
    if (info.bus_type == HID_API_BUS_I2C)
        score += 2;

    const std::wstring_view product = info.product_string ? info.product_string : L"";
    const std::wstring_view manufacturer =
        info.manufacturer_string ? info.manufacturer_string : L"";
    if (product.find(L"Touchpad") != std::wstring_view::npos)
        score += 2;
    if (product.find(L"UNIW") != std::wstring_view::npos)
        score += 2;
    if (manufacturer.find(L"UNIW") != std::wstring_view::npos)
        score += 1;
    return score;
}

std::string find_touchpad_path() {
    hid_device_info *list = hid_enumerate(0, 0);
    if (!list)
        return {};

    std::string best;
    int best_score = 0;
    for (auto *info = list; info; info = info->next) {
        const std::wstring_view product = info->product_string ? info->product_string : L"";
        const std::wstring_view manufacturer =
            info->manufacturer_string ? info->manufacturer_string : L"";
        if (!uni4linux::touchpad::is_touchpad(info->vendor_id, info->product_id, product,
                                              manufacturer))
            continue;

        const int score = selection_score(*info);
        if (info->path && score > best_score) {
            best = info->path;
            best_score = score;
        }
    }
    hid_free_enumeration(list);
    return best;
}

std::expected<HidHandle, uni4linux::touchpad::TouchpadError> open_touchpad() {
    const auto path = find_touchpad_path();
    if (path.empty())
        return std::unexpected(uni4linux::touchpad::TouchpadError::NotFound);

    hid_device *device = hid_open_path(path.c_str());
    if (!device)
        return std::unexpected(uni4linux::touchpad::TouchpadError::OpenFailed);
    return HidHandle{device};
}

} // namespace

namespace uni4linux::touchpad {

std::string_view describe(TouchpadError error) noexcept {
    switch (error) {
    case TouchpadError::NotFound:
        return "no PixArt/Uniwill touchpad found";
    case TouchpadError::OpenFailed:
        return "cannot open the touchpad hidraw device";
    case TouchpadError::ReadFailed:
        return "cannot read HID feature report 7";
    case TouchpadError::WriteFailed:
        return "cannot write HID feature report 7";
    }
    return "unknown touchpad error";
}

bool is_touchpad(std::uint16_t vendor_id, std::uint16_t product_id,
                 std::wstring_view product, std::wstring_view manufacturer) noexcept {
    if (vendor_id == kPixArtVendor || product_id == kTouchpadProduct)
        return true;
    return product.find(L"Touchpad") != std::wstring_view::npos ||
           product.find(L"UNIW") != std::wstring_view::npos ||
           manufacturer.find(L"UNIW") != std::wstring_view::npos ||
           manufacturer.find(L"PixArt") != std::wstring_view::npos;
}

std::expected<bool, TouchpadError> get_enabled() {
    auto handle = open_touchpad();
    if (!handle)
        return std::unexpected(handle.error());

    std::array<unsigned char, kReportBufferSize> buffer{};
    buffer[0] = kReportId;
    const int read = hid_get_feature_report(handle->get(), buffer.data(), buffer.size());
    if (read < 2)
        return std::unexpected(TouchpadError::ReadFailed);

    // bit 0 of the "Touchpad Configuration" byte: 1 = enabled.
    return (buffer[1] & 0x01) != 0;
}

std::expected<void, TouchpadError> set_enabled(bool enable) {
    auto handle = open_touchpad();
    if (!handle)
        return std::unexpected(handle.error());

    const std::array<unsigned char, 2> buffer{
        kReportId,
        static_cast<unsigned char>(enable ? 0x01 : 0x00),
    };
    if (hid_send_feature_report(handle->get(), buffer.data(), buffer.size()) < 0)
        return std::unexpected(TouchpadError::WriteFailed);
    return {};
}

std::expected<bool, TouchpadError> toggle() {
    auto current = get_enabled();
    if (!current)
        return std::unexpected(current.error());

    const bool next = !*current;
    if (auto result = set_enabled(next); !result)
        return std::unexpected(result.error());
    return next;
}

} // namespace uni4linux::touchpad
