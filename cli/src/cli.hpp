#pragma once

#include <CLI/CLI.hpp>

#include <cstdint>
#include <string>

namespace uni::cli {

struct Options {
    bool force{};
    std::string read_addr;
    std::string write_addr;
    std::string write_value;
    std::string dump_addr;
    std::string dump_length;

    int rgb_r{};
    int rgb_g{};
    int rgb_b{};
    int brightness{};
    std::string power_state;
    std::string effect_state;
    int breathe_period{3000};
    std::string boost_state;

    std::string keymap_scancode;
    std::string keymap_keycode;
};

struct Commands {
    CLI::App *probe{};

    CLI::App *ec_read{};
    CLI::App *ec_write{};
    CLI::App *ec_dump{};

    CLI::App *kbd_color{};
    CLI::App *kbd_brightness{};
    CLI::App *kbd_power{};
    CLI::App *kbd_effect{};
    CLI::App *kbd_off{};
    CLI::App *kbd_breathe{};
    CLI::App *kbd_test{};

    CLI::App *tp_get{};
    CLI::App *tp_on{};
    CLI::App *tp_off{};
    CLI::App *tp_toggle{};
    CLI::App *tp_watch{};

    CLI::App *fan_get{};
    CLI::App *fan_boost{};

    CLI::App *keymap{};
    CLI::App *monitor{};
};

void build_cli(CLI::App &app, Options &options, Commands &commands);
int dispatch(const Options &options, const Commands &commands);

} // namespace uni::cli
