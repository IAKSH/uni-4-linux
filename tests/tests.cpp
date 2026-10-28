#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ec.hpp"
#include "fan.hpp"
#include "hid.hpp"
#include "kbd.hpp"
#include "touchpad.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace uni4linux;

struct FakeEc {
    std::map<std::uint16_t, std::uint8_t> regs;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> writes;

    std::expected<std::uint8_t, EcError> read(std::uint16_t addr) {
        const auto it = regs.find(addr);
        if (it == regs.end())
            return std::unexpected(EcError::DeviceError);
        return it->second;
    }

    std::expected<void, EcError> write(std::uint16_t addr, std::uint8_t value) {
        regs[addr] = value;
        writes.emplace_back(addr, value);
        return {};
    }
};

struct FakeHid {
    std::map<std::uint8_t, std::vector<std::uint8_t>> reports;
    std::vector<std::pair<std::uint8_t, std::vector<std::uint8_t>>> writes;
    std::optional<hid::HidError> read_error;
    std::optional<hid::HidError> write_error;

    std::expected<std::size_t, hid::HidError>
    get_feature(std::uint8_t report_id, std::span<std::uint8_t> payload) {
        if (read_error)
            return std::unexpected(*read_error);
        const auto it = reports.find(report_id);
        if (it == reports.end())
            return std::unexpected(hid::HidError::ReadFailed);
        const auto count = std::min(payload.size(), it->second.size());
        std::copy_n(it->second.begin(), count, payload.begin());
        return count;
    }

    std::expected<void, hid::HidError>
    set_feature(std::uint8_t report_id, std::span<const std::uint8_t> payload) {
        if (write_error)
            return std::unexpected(*write_error);
        reports[report_id] = std::vector<std::uint8_t>(payload.begin(), payload.end());
        writes.emplace_back(report_id, reports[report_id]);
        return {};
    }
};

static_assert(hid::HidBackend<FakeHid>);

std::uint16_t address(Reg value) {
    return reg(value);
}

} // namespace

TEST_CASE("acpi_call text results are parsed") {
    CHECK(parse_ec_value("0x32").value() == 0x32);
    CHECK(parse_ec_value(" 0xFF \n").value() == 0xff);
    CHECK(parse_ec_value("50").value() == 50);
    CHECK(parse_ec_value("0x0").value() == 0);

    CHECK(parse_ec_value("").error() == EcError::ReadFailed);
    CHECK(parse_ec_value("0x100").error() == EcError::ReadFailed);
    CHECK(parse_ec_value("0xzz").error() == EcError::ReadFailed);
    CHECK(parse_ec_value("Error: AE_NOT_FOUND").error() == EcError::DeviceError);
    CHECK(parse_ec_value("not supported").error() == EcError::DeviceError);
}

TEST_CASE("missing acpi_call path is reported") {
    auto backend = AcpiEc::open("/definitely/not/here/uni4-acpi-call");
    REQUIRE_FALSE(backend.has_value());
    CHECK(backend.error() == EcError::CannotOpen);
}

TEST_CASE("0..255 RGB values are scaled to the EC's 0..50 range") {
    CHECK(kbd::scale_rgb(0) == 0);
    CHECK(kbd::scale_rgb(1) == 1);
    CHECK(kbd::scale_rgb(4) == 1);
    CHECK(kbd::scale_rgb(5) == 1);
    CHECK(kbd::scale_rgb(6) == 2);
    CHECK(kbd::scale_rgb(245) == 49);
    CHECK(kbd::scale_rgb(250) == 50);
    CHECK(kbd::scale_rgb(255) == 50);
}

TEST_CASE("Ec update and rewrite keep unrelated bits") {
    FakeEc fake;
    fake.regs[0x0100] = 0b1010'1010;
    Ec<FakeEc> ec{std::move(fake)};

    auto updated = ec.update(0x0100, 0x0f, 0x05);
    REQUIRE(updated.has_value());
    CHECK(ec.backend().regs.at(0x0100) == 0b1010'0101);
    CHECK(ec.backend().writes.size() == 1);

    auto rewritten = ec.rewrite(0x0100);
    REQUIRE(rewritten.has_value());
    CHECK(ec.backend().writes.back().second == 0b1010'0101);
}

TEST_CASE("Ec propagates backend errors") {
    FakeEc fake;
    Ec<FakeEc> ec{std::move(fake)};

    auto value = ec.read(0x1234);
    REQUIRE_FALSE(value.has_value());
    CHECK(value.error() == EcError::DeviceError);

    auto updated = ec.update(0x1234, 0x0f, 0x00);
    REQUIRE_FALSE(updated.has_value());
    CHECK(updated.error() == EcError::DeviceError);
}

TEST_CASE("setting a colour writes all channels and exits rainbow") {
    FakeEc fake;
    fake.regs[address(Reg::KbColorTrigger)] = kKbTriggerRainbow;
    Ec<FakeEc> ec{std::move(fake)};

    auto result = kbd::set_color(ec, kbd::Rgb{255, 0, 6});
    REQUIRE(result.has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbColorR)) == 50);
    CHECK(ec.backend().regs.at(address(Reg::KbColorG)) == 0);
    CHECK(ec.backend().regs.at(address(Reg::KbColorB)) == 2);
    CHECK(ec.backend().regs.at(address(Reg::KbColorTrigger)) == kKbTriggerRgb);
}

TEST_CASE("brightness preserves the low nibble and clamps to 0..4") {
    FakeEc fake;
    fake.regs[address(Reg::KbEnable)] = kKbPowerOff;
    Ec<FakeEc> ec{std::move(fake)};

    auto result = kbd::set_brightness(ec, 3);
    REQUIRE(result.has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbEnable)) ==
          (kKbPowerOff | (3 << 5) | kKbBrightnessApply));

    CHECK(kbd::set_brightness(ec, 99).has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbEnable)) ==
          (kKbPowerOff | (4 << 5) | kKbBrightnessApply));
}

TEST_CASE("power on restores the stored colour and preserves rainbow") {
    FakeEc fake;
    fake.regs[address(Reg::KbEnable)] = kKbPowerOff;
    fake.regs[address(Reg::KbColorTrigger)] = kKbTriggerRainbow;
    fake.regs[address(Reg::KbPowerPrep)] = 0x11;
    fake.regs[address(Reg::KbColorR)] = 10;
    fake.regs[address(Reg::KbColorG)] = 20;
    fake.regs[address(Reg::KbColorB)] = 30;
    Ec<FakeEc> ec{std::move(fake)};

    auto result = kbd::set_power(ec, true);
    REQUIRE(result.has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbEnable)) == 0x00);
    CHECK(ec.backend().regs.at(address(Reg::KbColorTrigger)) ==
          (kKbTriggerRainbow | kKbTriggerRgb));
    CHECK(ec.backend().regs.at(address(Reg::KbPowerPrep)) == 0x11);
    CHECK(ec.backend().regs.at(address(Reg::KbColorR)) == 10);
    CHECK(ec.backend().regs.at(address(Reg::KbColorG)) == 20);
    CHECK(ec.backend().regs.at(address(Reg::KbColorB)) == 30);
}

TEST_CASE("power off rewrites the trigger and sets the power bit") {
    FakeEc fake;
    fake.regs[address(Reg::KbEnable)] = 0x00;
    fake.regs[address(Reg::KbColorTrigger)] = kKbTriggerRgb;
    fake.regs[address(Reg::KbPowerPrep)] = 0x22;
    Ec<FakeEc> ec{std::move(fake)};

    auto result = kbd::set_power(ec, false);
    REQUIRE(result.has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbEnable)) == kKbPowerOff);
    CHECK(ec.backend().regs.at(address(Reg::KbColorTrigger)) == kKbTriggerRgb);
    CHECK(ec.backend().regs.at(address(Reg::KbPowerPrep)) == 0x22);
}

TEST_CASE("keyboard state decodes levels, brightness and power") {
    FakeEc fake;
    fake.regs[address(Reg::KbColorR)] = 50;
    fake.regs[address(Reg::KbColorG)] = 0;
    fake.regs[address(Reg::KbColorB)] = 0;
    fake.regs[address(Reg::KbEnable)] = 0x72;
    Ec<FakeEc> ec{std::move(fake)};

    auto state = kbd::get_state(ec);
    REQUIRE(state.has_value());
    CHECK(state->levels.r == 50);
    CHECK(state->levels.g == 0);
    CHECK(state->levels.b == 0);
    CHECK(state->brightness == 3);
    CHECK_FALSE(state->power);
}

TEST_CASE("keyboard probe reports a one-zone RGB keyboard") {
    FakeEc fake;
    fake.regs[address(Reg::ProjectId)] = 0x82;
    fake.regs[address(Reg::KeyboardId)] = 0x01;
    fake.regs[address(Reg::KbSupport)] = 0x04;
    Ec<FakeEc> ec{std::move(fake)};

    auto info = kbd::probe(ec);
    REQUIRE(info.has_value());
    CHECK(info->project_id == 0x82);
    CHECK(info->keyboard_id == 0x01);
    CHECK(info->type == kbd::KbdType::OneZoneRgb);
}

TEST_CASE("rainbow effect only touches the rainbow bit") {
    FakeEc fake;
    fake.regs[address(Reg::KbColorTrigger)] = 0x01;
    Ec<FakeEc> ec{std::move(fake)};

    REQUIRE(kbd::set_effect(ec, true).has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbColorTrigger)) ==
          (kKbTriggerRainbow | 0x01));

    REQUIRE(kbd::set_effect(ec, false).has_value());
    CHECK(ec.backend().regs.at(address(Reg::KbColorTrigger)) == 0x01);
}

TEST_CASE("fan boost preserves unrelated mode bits") {
    FakeEc fake;
    fake.regs[address(Reg::FanMode)] = kFanManual;
    Ec<FakeEc> ec{std::move(fake)};

    REQUIRE(fan::set_boost(ec, true).has_value());
    CHECK(ec.backend().regs.at(address(Reg::FanMode)) == (kFanManual | kFanBoost));

    REQUIRE(fan::set_boost(ec, false).has_value());
    CHECK(ec.backend().regs.at(address(Reg::FanMode)) == kFanManual);
}

TEST_CASE("fan telemetry combines high and low RPM bytes") {
    FakeEc fake;
    fake.regs[address(Reg::FanMode)] = kFanBoost;
    fake.regs[address(Reg::FanLeftDuty)] = 100;
    fake.regs[address(Reg::FanRightDuty)] = 200;
    fake.regs[address(Reg::FanLeftRpmHi)] = 0x12;
    fake.regs[address(Reg::FanLeftRpmLo)] = 0x34;
    fake.regs[address(Reg::FanRightRpmHi)] = 0x56;
    fake.regs[address(Reg::FanRightRpmLo)] = 0x78;
    Ec<FakeEc> ec{std::move(fake)};

    auto state = fan::get(ec);
    REQUIRE(state.has_value());
    CHECK(state->mode == kFanBoost);
    CHECK(state->left_duty == 100);
    CHECK(state->right_duty == 200);
    CHECK(state->left_rpm == 0x1234);
    CHECK(state->right_rpm == 0x5678);
    CHECK(fan::combine_rpm(0xab, 0xcd) == 0xabcd);
}

TEST_CASE("touchpad selection accepts PixArt and Uniwill names") {
    CHECK(touchpad::is_touchpad(0x093a, 0x0255, L"", L""));
    CHECK(touchpad::is_touchpad(0x0000, 0x0255, L"", L""));
    CHECK(touchpad::is_touchpad(0x0000, 0x0000, L"Touchpad", L""));
    CHECK(touchpad::is_touchpad(0x0000, 0x0000, L"", L"UNIW"));
    CHECK(touchpad::is_touchpad(0x0000, 0x0000, L"", L"PixArt"));
    CHECK_FALSE(touchpad::is_touchpad(0x1234, 0x5678, L"Mouse", L"Acme"));
}

TEST_CASE("touchpad feature reports use the injected HID backend") {
    FakeHid fake;
    fake.reports[touchpad::kReportId] = {0x01};

    auto enabled = touchpad::get_enabled(fake);
    REQUIRE(enabled.has_value());
    CHECK(*enabled);

    auto disabled = touchpad::set_enabled(fake, false);
    REQUIRE(disabled.has_value());
    REQUIRE(fake.reports.at(touchpad::kReportId).size() == 1);
    CHECK(fake.reports.at(touchpad::kReportId)[0] == 0x00);
    REQUIRE(fake.writes.size() == 1);
    CHECK(fake.writes.front().first == touchpad::kReportId);
    REQUIRE(fake.writes.front().second.size() == 1);
    CHECK(fake.writes.front().second[0] == 0x00);

    auto off = touchpad::get_enabled(fake);
    REQUIRE(off.has_value());
    CHECK_FALSE(*off);

    auto toggled = touchpad::toggle(fake);
    REQUIRE(toggled.has_value());
    CHECK(*toggled);
    REQUIRE(fake.reports.at(touchpad::kReportId).size() == 1);
    CHECK(fake.reports.at(touchpad::kReportId)[0] == 0x01);
}

TEST_CASE("touchpad reports missing or short feature reports") {
    FakeHid fake;
    auto missing = touchpad::get_enabled(fake);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == touchpad::TouchpadError::ReadFailed);

    fake.reports[touchpad::kReportId] = {};
    auto empty = touchpad::get_enabled(fake);
    REQUIRE_FALSE(empty.has_value());
    CHECK(empty.error() == touchpad::TouchpadError::ReadFailed);
}

TEST_CASE("touchpad selection accepts enumerated HID info") {
    hid::HidDeviceInfo touchpad_device;
    touchpad_device.vendor_id = 0x093a;
    touchpad_device.product_id = 0x0255;
    touchpad_device.bus = hid::HidBus::I2c;
    touchpad_device.product = L"Touchpad";
    CHECK(touchpad::is_touchpad(touchpad_device));

    hid::HidDeviceInfo mouse;
    mouse.vendor_id = 0x1234;
    mouse.product_id = 0x5678;
    mouse.product = L"Mouse";
    CHECK_FALSE(touchpad::is_touchpad(mouse));
}

TEST_CASE("hid adapter rejects an empty device path") {
    auto device = hid::HidApi::open("");
    REQUIRE_FALSE(device.has_value());
    CHECK(device.error() == hid::HidError::OpenFailed);
}

TEST_CASE("touchpad selection prefers the I2C PixArt touchpad") {
    hid::HidDeviceInfo mouse{
        0x093a, 0x1234, hid::HidBus::Usb, "/dev/hidraw0", L"Mouse", L"PixArt",
    };
    hid::HidDeviceInfo touchpad_device{
        0x093a, 0x0255, hid::HidBus::I2c, "/dev/hidraw1", L"Touchpad", L"UNIW",
    };
    const std::vector<hid::HidDeviceInfo> devices{mouse, touchpad_device};

    const auto *selected = touchpad::select_touchpad(devices);
    REQUIRE(selected != nullptr);
    CHECK(selected->path == "/dev/hidraw1");
}

TEST_CASE("touchpad selection returns null when only non-touchpads exist") {
    const std::vector<hid::HidDeviceInfo> devices{
        hid::HidDeviceInfo{0x1234, 0x5678, hid::HidBus::Usb, "/dev/hidraw0", L"Mouse",
                           L"Acme"},
    };
    CHECK(touchpad::select_touchpad(devices) == nullptr);
}

TEST_CASE("touchpad maps HID backend failures") {
    FakeHid fake;
    fake.read_error = hid::HidError::OpenFailed;
    auto read = touchpad::get_enabled(fake);
    REQUIRE_FALSE(read.has_value());
    CHECK(read.error() == touchpad::TouchpadError::OpenFailed);

    fake.read_error.reset();
    fake.write_error = hid::HidError::WriteFailed;
    auto write = touchpad::set_enabled(fake, true);
    REQUIRE_FALSE(write.has_value());
    CHECK(write.error() == touchpad::TouchpadError::WriteFailed);
}

TEST_CASE("touchpad selection ignores entries without a device path") {
    const std::vector<hid::HidDeviceInfo> devices{
        hid::HidDeviceInfo{0x093a, 0x0255, hid::HidBus::I2c, "", L"Touchpad", L""},
        hid::HidDeviceInfo{0x093a, 0x0255, hid::HidBus::I2c, "/dev/hidraw1",
                           L"Touchpad", L""},
    };
    const auto *selected = touchpad::select_touchpad(devices);
    REQUIRE(selected != nullptr);
    CHECK(selected->path == "/dev/hidraw1");
}
