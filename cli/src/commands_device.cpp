#include "commands.hpp"
#include "input.hpp"
#include "support.hpp"
#include "util.hpp"

#include <uni/fan.hpp>
#include <uni/kbd.hpp>
#include <uni/touchpad.hpp>

#include <cstdint>
#include <expected>

namespace uni::cli {

int probe() {
    return with_ec([](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        auto info = keyboard.probe();
        if (!info)
            return std::unexpected(info.error());

        println("ec: project_id 0x{:02x} keyboard_id 0x{:02x}",
                static_cast<unsigned>(info->project_id),
                static_cast<unsigned>(info->keyboard_id));
        println("backlight type: {}", type_name(info->type));

        auto state = keyboard.state();
        if (!state)
            return std::unexpected(state.error());
        println("backlight: R={} G={} B={} brightness={} power={}",
                static_cast<unsigned>(state->levels.r),
                static_cast<unsigned>(state->levels.g),
                static_cast<unsigned>(state->levels.b), state->brightness,
                state->power ? "on" : "off");

        auto touchpad_device = touchpad::open();
        if (touchpad_device) {
            auto enabled = touchpad_device->is_enabled();
            if (enabled)
                println("touchpad: {}", *enabled ? "enabled" : "disabled");
            else
                println("touchpad: {}", touchpad::describe(enabled.error()));
        } else {
            println("touchpad: {}", touchpad::describe(touchpad_device.error()));
        }
        return {};
    });
}

int tp_get() {
    auto device = touchpad::open();
    if (!device) {
        eprintln("touchpad: {}", touchpad::describe(device.error()));
        return 1;
    }
    auto state = device->is_enabled();
    if (!state) {
        eprintln("touchpad: {}", touchpad::describe(state.error()));
        return 1;
    }
    println("{}", *state ? 1 : 0);
    return 0;
}

int tp_set(bool enable) {
    auto device = touchpad::open();
    if (!device) {
        eprintln("touchpad: {}", touchpad::describe(device.error()));
        return 1;
    }
    auto result = device->set_enabled(enable);
    if (!result) {
        eprintln("touchpad: {}", touchpad::describe(result.error()));
        return 1;
    }
    println("touchpad {}", enable ? "enabled" : "disabled");
    return 0;
}

int tp_toggle() {
    auto device = touchpad::open();
    if (!device) {
        eprintln("touchpad: {}", touchpad::describe(device.error()));
        return 1;
    }
    auto state = device->toggle();
    if (!state) {
        eprintln("touchpad: {}", touchpad::describe(state.error()));
        return 1;
    }
    println("touchpad {}", *state ? "enabled" : "disabled");
    return 0;
}

int tp_watch() {
    auto result = input::watch_touchpad();
    if (!result) {
        eprintln("tp watch: {}", input::describe(result.error()));
        return 1;
    }
    return 0;
}

int fan_get() {
    return with_ec([](auto &ec) -> std::expected<void, EcError> {
        fan::Fan fan_device{ec};
        auto state = fan_device.state();
        if (!state)
            return std::unexpected(state.error());

        println("mode: 0x{:02x}{}{}", static_cast<unsigned>(state->mode),
                (state->mode & kFanBoost) ? " (boost)" : "",
                (state->mode & kFanManual) ? " (manual)" : "");
        println("duty: left {}% right {}%", state->left_duty / 2,
                state->right_duty / 2);
        println("rpm:  left {} right {}", state->left_rpm, state->right_rpm);
        return {};
    });
}

int fan_boost(bool on, bool force) {
    return with_ec(force, [on](auto &ec) -> std::expected<void, EcError> {
        fan::Fan fan_device{ec};
        if (auto result = fan_device.set_boost(on); !result)
            return std::unexpected(result.error());
        println("fan boost {}", on ? "on" : "off");
        return {};
    });
}

} // namespace uni::cli
