#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace uni::hid {

enum class HidError {
    OpenFailed,
    ReadFailed,
    WriteFailed,
};

std::string_view describe(HidError error) noexcept;

enum class HidBus {
    Unknown,
    Usb,
    Bluetooth,
    I2c,
    Spi,
};

struct HidDeviceInfo {
    std::uint16_t vendor_id{};
    std::uint16_t product_id{};
    HidBus bus{HidBus::Unknown};
    std::string path;
    std::wstring product;
    std::wstring manufacturer;
};

// Enumerate every HID device.  hidapi does not distinguish "no devices" from
// an enumeration failure, so an empty vector is returned in both cases.
std::vector<HidDeviceInfo> enumerate();

// Production backend: hidapi/hidraw.  This header intentionally does not
// include <hidapi/hidapi.h>; that dependency stays in hid.cpp.
class HidApi {
public:
    static std::expected<HidApi, HidError> open(std::string_view path);

    HidApi(const HidApi &) = delete;
    HidApi &operator=(const HidApi &) = delete;
    HidApi(HidApi &&other) noexcept;
    HidApi &operator=(HidApi &&other) noexcept;
    ~HidApi();

    // payload excludes the report ID; the adapter prepends it.
    [[nodiscard]] std::expected<std::size_t, HidError>
    get_feature(std::uint8_t report_id, std::span<std::uint8_t> payload) noexcept;

    [[nodiscard]] std::expected<void, HidError>
    set_feature(std::uint8_t report_id, std::span<const std::uint8_t> payload) noexcept;

private:
    struct Impl;

    explicit HidApi(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

template <typename T>
concept HidBackend = requires(T &device, std::uint8_t report_id,
                              std::span<std::uint8_t> payload,
                              std::span<const std::uint8_t> input) {
    { device.get_feature(report_id, payload) }
        -> std::same_as<std::expected<std::size_t, HidError>>;
    { device.set_feature(report_id, input) }
        -> std::same_as<std::expected<void, HidError>>;
};

static_assert(HidBackend<HidApi>);

} // namespace uni::hid
