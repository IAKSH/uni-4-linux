#include "model.hpp"

#include <uni/fan.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <string>
#include <utility>

namespace uni::tui {
namespace {

constexpr std::array<kbd::Rgb, 8> kPresets{{
    {255, 0, 0},
    {255, 128, 0},
    {255, 255, 0},
    {0, 255, 0},
    {0, 255, 255},
    {0, 0, 255},
    {255, 0, 255},
    {255, 255, 255},
}};

} // namespace

void Model::refresh() {
    snapshot_ = Snapshot{};
    snapshot_.updated = std::chrono::system_clock::now();
    snapshot_.force_writes = force_writes_;

    if (ensure_ec()) {
        machine_ = detect_machine(*ec_);
        snapshot_.machine = machine_;

        kbd::Keyboard keyboard{*ec_};

        auto info = keyboard.probe();
        if (info) {
            snapshot_.keyboard.available = true;
            snapshot_.keyboard.project_id = info->project_id;
            snapshot_.keyboard.keyboard_id = info->keyboard_id;
            snapshot_.keyboard.type = info->type;
        } else {
            snapshot_.keyboard.error = std::string(describe(info.error()));
        }

        auto state = keyboard.state();
        if (state) {
            snapshot_.keyboard.available = true;
            snapshot_.keyboard.levels = state->levels;
            snapshot_.keyboard.brightness = state->brightness;
            snapshot_.keyboard.power = state->power;
            snapshot_.keyboard.rainbow = state->rainbow;
        } else {
            snapshot_.keyboard.error = std::string(describe(state.error()));
        }

        fan::Fan fan_device{*ec_};
        auto fan_state = fan_device.state();
        if (fan_state) {
            snapshot_.fan.available = true;
            snapshot_.fan.mode = fan_state->mode;
            snapshot_.fan.left_duty = fan_state->left_duty;
            snapshot_.fan.right_duty = fan_state->right_duty;
            snapshot_.fan.left_rpm = fan_state->left_rpm;
            snapshot_.fan.right_rpm = fan_state->right_rpm;
        } else {
            snapshot_.fan.error = std::string(describe(fan_state.error()));
        }
    } else {
        machine_ = check_machine(read_machine_info(), std::nullopt);
        snapshot_.machine = machine_;
        snapshot_.ec_error = ec_error_;
    }

    if (ensure_touchpad()) {
        auto enabled = touchpad_->is_enabled();
        if (enabled) {
            snapshot_.touchpad.available = true;
            snapshot_.touchpad.enabled = *enabled;
        } else {
            snapshot_.touchpad.error = std::string(touchpad::describe(enabled.error()));
            touchpad_.reset();
        }
    } else {
        snapshot_.touchpad.error = touchpad_error_;
    }
}

void Model::set_force_writes(bool force) {
    force_writes_ = force;
    snapshot_.force_writes = force;
    set_status(force ? "EC writes forced" : "EC writes restricted", !force);
}

void Model::set_color(kbd::Rgb rgb) {
    if (!guard_write())
        return;
    with_keyboard([rgb](auto &keyboard) { return keyboard.set_color(rgb); },
                  std::format("colour {}, {}, {}", static_cast<unsigned>(rgb.r),
                              static_cast<unsigned>(rgb.g),
                              static_cast<unsigned>(rgb.b)));
}

void Model::set_brightness(int brightness) {
    if (!guard_write())
        return;
    with_keyboard(
        [brightness](auto &keyboard) {
            return keyboard.set_brightness(brightness);
        },
        std::format("brightness {}", brightness));
}

void Model::adjust_brightness(int delta) {
    const int current = snapshot_.keyboard.available ? snapshot_.keyboard.brightness : 3;
    set_brightness(std::clamp(current + delta, 0, 4));
}

void Model::toggle_power() {
    if (!guard_write())
        return;
    const bool next = !snapshot_.keyboard.power;
    with_keyboard([next](auto &keyboard) { return keyboard.set_power(next); },
                  next ? "backlight power on" : "backlight power off");
}

void Model::toggle_effect() {
    if (!guard_write())
        return;
    const bool next = !snapshot_.keyboard.rainbow;
    with_keyboard([next](auto &keyboard) { return keyboard.set_effect(next); },
                  next ? "firmware rainbow on" : "firmware rainbow off");
}

void Model::cycle_color() {
    const auto rgb = kPresets[color_index_ % kPresets.size()];
    ++color_index_;
    set_color(rgb);
}

void Model::toggle_fan_boost() {
    if (!guard_write())
        return;
    if (!ensure_ec()) {
        set_status(ec_error_, true);
        return;
    }

    fan::Fan fan_device{*ec_};
    const bool next = (snapshot_.fan.mode & kFanBoost) == 0;
    auto result = fan_device.set_boost(next);
    if (!result) {
        set_status(std::string(describe(result.error())), true);
        return;
    }
    set_status(next ? "fan boost on" : "fan boost off", false);
    refresh();
}

void Model::toggle_touchpad() {
    if (!ensure_touchpad()) {
        set_status(touchpad_error_, true);
        return;
    }

    auto result = touchpad_->toggle();
    if (!result) {
        set_status(std::string(touchpad::describe(result.error())), true);
        touchpad_.reset();
        refresh();
        return;
    }
    set_status(*result ? "touchpad enabled" : "touchpad disabled", false);
    refresh();
}

bool Model::ensure_ec() {
    if (ec_)
        return true;

    auto opened = AcpiEc::open();
    if (!opened) {
        ec_error_ = std::string(describe(opened.error()));
        return false;
    }
    ec_.emplace(std::move(*opened));
    ec_error_.clear();
    return true;
}

bool Model::ensure_touchpad() {
    if (touchpad_)
        return true;

    auto opened = touchpad::open();
    if (!opened) {
        touchpad_error_ = std::string(touchpad::describe(opened.error()));
        return false;
    }
    touchpad_.emplace(std::move(*opened));
    touchpad_error_.clear();
    return true;
}

bool Model::guard_write() {
    if (machine_.writes_allowed() || force_writes_)
        return true;

    set_status("target machine check failed; press F to allow writes", true);
    return false;
}

void Model::set_status(std::string message, bool error) {
    status_ = std::move(message);
    status_error_ = error;
}

} // namespace uni::tui
