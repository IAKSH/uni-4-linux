#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ec.hpp"
#include "fan.hpp"
#include "kbd.hpp"
#include "touchpad.hpp"

#include <cstdint>
#include <expected>
#include <map>
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
