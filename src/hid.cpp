#include "hid.hpp"

#include <hidapi/hidapi.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

uni::hid::HidBus map_bus(hid_bus_type bus) noexcept {
    switch (bus) {
    case HID_API_BUS_UNKNOWN:
        return uni::hid::HidBus::Unknown;
    case HID_API_BUS_USB:
        return uni::hid::HidBus::Usb;
    case HID_API_BUS_BLUETOOTH:
        return uni::hid::HidBus::Bluetooth;
    case HID_API_BUS_I2C:
        return uni::hid::HidBus::I2c;
    case HID_API_BUS_SPI:
        return uni::hid::HidBus::Spi;
    }
    return uni::hid::HidBus::Unknown;
}

} // namespace

struct uni::hid::HidApi::Impl {
    hid_device *handle{};

    explicit Impl(hid_device *device) : handle(device) {}

    ~Impl() {
        if (handle)
            hid_close(handle);
    }
};

namespace uni::hid {

HidApi::HidApi(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
HidApi::HidApi(HidApi &&other) noexcept = default;
HidApi &HidApi::operator=(HidApi &&other) noexcept = default;
HidApi::~HidApi() = default;

std::string_view describe(HidError error) noexcept {
    switch (error) {
    case HidError::OpenFailed:
        return "cannot open the HID device";
    case HidError::ReadFailed:
        return "cannot read a HID feature report";
    case HidError::WriteFailed:
        return "cannot write a HID feature report";
    }
    return "unknown HID error";
}

std::vector<HidDeviceInfo> enumerate() {
    std::vector<HidDeviceInfo> result;
    hid_device_info *list = hid_enumerate(0, 0);
    for (auto *info = list; info; info = info->next) {
        HidDeviceInfo device;
        device.vendor_id = info->vendor_id;
        device.product_id = info->product_id;
        device.bus = map_bus(info->bus_type);
        if (info->path)
            device.path = info->path;
        if (info->product_string)
            device.product = info->product_string;
        if (info->manufacturer_string)
            device.manufacturer = info->manufacturer_string;
        result.push_back(std::move(device));
    }
    if (list)
        hid_free_enumeration(list);
    return result;
}

std::expected<HidApi, HidError> HidApi::open(std::string_view path) {
    if (path.empty())
        return std::unexpected(HidError::OpenFailed);

    hid_device *handle = hid_open_path(std::string(path).c_str());
    if (!handle)
        return std::unexpected(HidError::OpenFailed);
    return HidApi{std::make_unique<Impl>(handle)};
}

std::expected<std::size_t, HidError>
HidApi::get_feature(std::uint8_t report_id, std::span<std::uint8_t> payload) noexcept {
    if (!impl_ || !impl_->handle)
        return std::unexpected(HidError::OpenFailed);

    std::vector<unsigned char> buffer(payload.size() + 1);
    buffer[0] = report_id;
    const int count = hid_get_feature_report(impl_->handle, buffer.data(), buffer.size());
    if (count < 0)
        return std::unexpected(HidError::ReadFailed);

    const auto total = static_cast<std::size_t>(count);
    const auto data_size = total > 0 ? total - 1 : 0;
    const auto copied = std::min(payload.size(), data_size);
    std::copy_n(buffer.begin() + 1, copied, payload.begin());
    return copied;
}

std::expected<void, HidError>
HidApi::set_feature(std::uint8_t report_id, std::span<const std::uint8_t> payload) noexcept {
    if (!impl_ || !impl_->handle)
        return std::unexpected(HidError::OpenFailed);

    std::vector<unsigned char> buffer(payload.size() + 1);
    buffer[0] = report_id;
    std::copy(payload.begin(), payload.end(), buffer.begin() + 1);
    if (hid_send_feature_report(impl_->handle, buffer.data(), buffer.size()) < 0)
        return std::unexpected(HidError::WriteFailed);
    return {};
}

} // namespace uni::hid
