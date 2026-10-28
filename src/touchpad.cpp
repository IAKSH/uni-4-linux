#include "touchpad.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace uni4linux::touchpad {
namespace {

constexpr std::uint16_t kPixArtVendor = 0x093a;
constexpr std::uint16_t kTouchpadProduct = 0x0255;

int selection_score(const hid::HidDeviceInfo &device) noexcept {
    int score = 0;
    if (device.vendor_id == kPixArtVendor)
        score += 4;
    if (device.product_id == kTouchpadProduct)
        score += 2;
    if (device.bus == hid::HidBus::I2c)
        score += 2;
    if (device.product.find(L"Touchpad") != std::wstring::npos)
        score += 2;
    if (device.product.find(L"UNIW") != std::wstring::npos)
        score += 2;
    if (device.manufacturer.find(L"UNIW") != std::wstring::npos)
        score += 1;
    return score;
}

std::expected<hid::HidApi, TouchpadError> open_touchpad() {
    const auto devices = hid::enumerate();
    const auto *best = select_touchpad(devices);
    if (!best)
        return std::unexpected(TouchpadError::NotFound);

    auto handle = hid::HidApi::open(best->path);
    if (!handle)
        return std::unexpected(from_hid(handle.error()));
    return std::move(*handle);
}

} // namespace

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

bool is_touchpad(const hid::HidDeviceInfo &device) noexcept {
    return is_touchpad(device.vendor_id, device.product_id, device.product,
                       device.manufacturer);
}

const hid::HidDeviceInfo *
select_touchpad(std::span<const hid::HidDeviceInfo> devices) noexcept {
    const hid::HidDeviceInfo *best = nullptr;
    int best_score = 0;
    for (const auto &device : devices) {
        if (device.path.empty() || !is_touchpad(device))
            continue;
        const int score = selection_score(device);
        if (!best || score > best_score) {
            best = &device;
            best_score = score;
        }
    }
    return best;
}

std::expected<bool, TouchpadError> get_enabled() {
    auto device = open_touchpad();
    if (!device)
        return std::unexpected(device.error());
    return get_enabled(*device);
}

std::expected<void, TouchpadError> set_enabled(bool enable) {
    auto device = open_touchpad();
    if (!device)
        return std::unexpected(device.error());
    return set_enabled(*device, enable);
}

std::expected<bool, TouchpadError> toggle() {
    auto device = open_touchpad();
    if (!device)
        return std::unexpected(device.error());
    return toggle(*device);
}

} // namespace uni4linux::touchpad
