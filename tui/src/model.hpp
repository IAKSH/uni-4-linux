#pragma once

#include <uni/ec.hpp>
#include <uni/kbd.hpp>
#include <uni/machine.hpp>
#include <uni/touchpad.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace uni::tui {

struct KeyboardSnapshot {
    bool available{};
    std::uint8_t project_id{};
    std::uint8_t keyboard_id{};
    kbd::KbdType type{kbd::KbdType::None};
    kbd::Rgb levels{};
    int brightness{};
    bool power{};
    bool rainbow{};
    std::string error;
};

struct FanSnapshot {
    bool available{};
    std::uint8_t mode{};
    std::uint8_t left_duty{};
    std::uint8_t right_duty{};
    std::uint16_t left_rpm{};
    std::uint16_t right_rpm{};
    std::string error;
};

struct TouchpadSnapshot {
    bool available{};
    bool enabled{};
    std::string error;
};

struct Snapshot {
    KeyboardSnapshot keyboard;
    FanSnapshot fan;
    TouchpadSnapshot touchpad;
    MachineCheck machine;
    bool force_writes{};
    std::string ec_error;
    std::chrono::system_clock::time_point updated{};
};

class Model {
public:
    explicit Model(bool force_writes = false)
        : force_writes_(force_writes) {}

    void refresh();
    void set_force_writes(bool force);

    void set_color(kbd::Rgb rgb);
    void set_brightness(int brightness);
    void adjust_brightness(int delta);
    void toggle_power();
    void toggle_effect();
    void cycle_color();
    void toggle_fan_boost();
    void toggle_touchpad();

    [[nodiscard]] const Snapshot &snapshot() const noexcept { return snapshot_; }
    [[nodiscard]] std::string_view status() const noexcept { return status_; }
    [[nodiscard]] bool status_error() const noexcept { return status_error_; }

private:
    bool ensure_ec();
    bool ensure_touchpad();
    bool guard_write();
    void set_status(std::string message, bool error);

    template <typename Fn>
    void with_keyboard(Fn &&fn, std::string success) {
        if (!ensure_ec()) {
            set_status(ec_error_, true);
            return;
        }

        kbd::Keyboard keyboard{*ec_};
        auto result = fn(keyboard);
        if (!result) {
            set_status(std::string(describe(result.error())), true);
            return;
        }
        set_status(std::move(success), false);
        refresh();
    }

    std::optional<Ec<AcpiEc>> ec_;
    std::optional<touchpad::HidTouchpad> touchpad_;
    MachineInfo machine_info_{read_machine_info()};
    MachineCheck machine_;
    Snapshot snapshot_;
    std::string ec_error_;
    std::string touchpad_error_;
    std::string status_{"ready"};
    bool status_error_{};
    bool force_writes_{};
    std::size_t color_index_{};
};

} // namespace uni::tui
