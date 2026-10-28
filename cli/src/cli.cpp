#include "cli.hpp"

#include "commands.hpp"
#include "support.hpp"
#include "util.hpp"

#include <CLI/CLI.hpp>

#include <cstdint>
#include <string>

namespace uni::cli {
namespace {

void add_rgb_options(CLI::App &command, int &red, int &green, int &blue) {
    command.add_option("r", red, "red 0-255")->required()->check(CLI::Range(0, 255));
    command.add_option("g", green, "green 0-255")->required()->check(CLI::Range(0, 255));
    command.add_option("b", blue, "blue 0-255")->required()->check(CLI::Range(0, 255));
}

void add_on_off_option(CLI::App &command, std::string &state) {
    command.add_option("state", state, "on or off")
        ->required()
        ->check(CLI::IsMember({"on", "off"}));
}

} // namespace

void build_cli(CLI::App &app, Options &options, Commands &commands) {
    commands.probe = app.add_subcommand("probe", "Print EC/backlight/touchpad state");

    auto *ec = app.add_subcommand("ec", "Raw EC register access");
    ec->require_subcommand(1);

    commands.ec_read = ec->add_subcommand("read", "Read one EC byte");
    commands.ec_read->add_option("addr", options.read_addr,
                                 "16-bit address (e.g. 0x0769)")
        ->required();

    commands.ec_write = ec->add_subcommand("write", "Write one EC byte");
    commands.ec_write->add_option("addr", options.write_addr, "16-bit address")
        ->required();
    commands.ec_write->add_option("value", options.write_value,
                                  "byte value (0-255 or 0x00-0xff)")
        ->required();

    commands.ec_dump = ec->add_subcommand("dump", "Dump EC bytes");
    commands.ec_dump->add_option("addr", options.dump_addr, "start address")
        ->required();
    commands.ec_dump->add_option("len", options.dump_length, "number of bytes")
        ->required();

    auto *kbd_cmd = app.add_subcommand("kbd", "Keyboard backlight");
    kbd_cmd->require_subcommand(1);

    commands.kbd_color = kbd_cmd->add_subcommand("color", "Set static colour (0-255 each)");
    add_rgb_options(*commands.kbd_color, options.rgb_r, options.rgb_g, options.rgb_b);

    commands.kbd_brightness =
        kbd_cmd->add_subcommand("brightness", "Set brightness 0-4");
    commands.kbd_brightness->add_option("level", options.brightness, "0 (min) .. 4 (max)")
        ->required()
        ->check(CLI::Range(0, 4));

    commands.kbd_power =
        kbd_cmd->add_subcommand("power", "Keyboard backlight master power");
    add_on_off_option(*commands.kbd_power, options.power_state);

    commands.kbd_effect = kbd_cmd->add_subcommand("effect", "Firmware light effect");
    commands.kbd_effect->add_option("effect", options.effect_state, "rainbow or static")
        ->required()
        ->check(CLI::IsMember({"rainbow", "static"}));

    commands.kbd_off = kbd_cmd->add_subcommand("off", "Turn the backlight off");

    commands.kbd_breathe =
        kbd_cmd->add_subcommand("breathe", "Software breathing effect");
    add_rgb_options(*commands.kbd_breathe, options.rgb_r, options.rgb_g, options.rgb_b);
    commands.kbd_breathe->add_option("period", options.breathe_period,
                                     "cycle in milliseconds")
        ->check(CLI::Range(1, 600000));

    commands.kbd_test =
        kbd_cmd->add_subcommand("test", "Write a colour and dump registers");
    add_rgb_options(*commands.kbd_test, options.rgb_r, options.rgb_g, options.rgb_b);

    auto *tp_cmd = app.add_subcommand("tp", "Touchpad switch and LED");
    tp_cmd->require_subcommand(1);
    commands.tp_get = tp_cmd->add_subcommand("get", "Print touchpad state (1 = enabled)");
    commands.tp_on = tp_cmd->add_subcommand("on", "Enable the touchpad");
    commands.tp_off = tp_cmd->add_subcommand("off", "Disable the touchpad");
    commands.tp_toggle = tp_cmd->add_subcommand("toggle", "Toggle the touchpad");
    commands.tp_watch = tp_cmd->add_subcommand("watch", "Toggle on Fn+F5 or KEY_F21");

    auto *fan_cmd = app.add_subcommand("fan", "Fan telemetry and boost");
    fan_cmd->require_subcommand(1);
    commands.fan_get = fan_cmd->add_subcommand("get", "Read fan mode/duty/RPM");
    commands.fan_boost = fan_cmd->add_subcommand("boost", "Fan boost (full speed)");
    add_on_off_option(*commands.fan_boost, options.boost_state);

    commands.keymap = app.add_subcommand("keymap", "Map a scancode on the AT keyboard");
    commands.keymap->add_option("scancode", options.keymap_scancode,
                                "hex scancode (e.g. f8)")
        ->required();
    commands.keymap->add_option("keycode", options.keymap_keycode,
                                "evdev keycode (e.g. 350)")
        ->required();

    commands.monitor = app.add_subcommand("monitor", "Print input key events");
}

int dispatch(const Options &options, const Commands &commands) {
    if (commands.probe->count() > 0)
        return probe();

    if (commands.ec_read->count() > 0) {
        const auto addr = parse_number<std::uint16_t>(options.read_addr);
        if (!addr) {
            eprintln("invalid address: {}", options.read_addr);
            return 2;
        }
        return ec_read(*addr);
    }

    if (commands.ec_write->count() > 0) {
        const auto addr = parse_number<std::uint16_t>(options.write_addr);
        const auto value = parse_number<std::uint8_t>(options.write_value);
        if (!addr || !value) {
            eprintln("invalid address or value");
            return 2;
        }
        return ec_write(*addr, *value);
    }

    if (commands.ec_dump->count() > 0) {
        const auto addr = parse_number<std::uint16_t>(options.dump_addr);
        const auto length = parse_number<std::uint32_t>(options.dump_length);
        if (!addr || !length || *length == 0 ||
            static_cast<std::uint32_t>(*addr) + *length > 0x10000u) {
            eprintln("invalid address or length");
            return 2;
        }
        return ec_dump(*addr, *length);
    }

    if (commands.kbd_color->count() > 0)
        return kbd_color(kbd::Rgb{
            static_cast<std::uint8_t>(options.rgb_r),
            static_cast<std::uint8_t>(options.rgb_g),
            static_cast<std::uint8_t>(options.rgb_b),
        });

    if (commands.kbd_brightness->count() > 0)
        return kbd_brightness(options.brightness);

    if (commands.kbd_power->count() > 0)
        return kbd_power(options.power_state == "on");

    if (commands.kbd_effect->count() > 0)
        return kbd_effect(options.effect_state == "rainbow");

    if (commands.kbd_off->count() > 0)
        return kbd_off();

    if (commands.kbd_breathe->count() > 0)
        return kbd_breathe(kbd::Rgb{
                               static_cast<std::uint8_t>(options.rgb_r),
                               static_cast<std::uint8_t>(options.rgb_g),
                               static_cast<std::uint8_t>(options.rgb_b),
                           },
                           options.breathe_period);

    if (commands.kbd_test->count() > 0)
        return kbd_test(kbd::Rgb{
            static_cast<std::uint8_t>(options.rgb_r),
            static_cast<std::uint8_t>(options.rgb_g),
            static_cast<std::uint8_t>(options.rgb_b),
        });

    if (commands.tp_get->count() > 0)
        return tp_get();
    if (commands.tp_on->count() > 0)
        return tp_set(true);
    if (commands.tp_off->count() > 0)
        return tp_set(false);
    if (commands.tp_toggle->count() > 0)
        return tp_toggle();
    if (commands.tp_watch->count() > 0)
        return tp_watch();

    if (commands.fan_get->count() > 0)
        return fan_get();
    if (commands.fan_boost->count() > 0)
        return fan_boost(options.boost_state == "on");

    if (commands.keymap->count() > 0) {
        const auto scancode = parse_scancode(options.keymap_scancode);
        const auto keycode = parse_number<std::uint16_t>(options.keymap_keycode);
        if (!scancode || !keycode) {
            eprintln("invalid scancode or keycode");
            return 2;
        }
        return keymap(*scancode, *keycode);
    }

    if (commands.monitor->count() > 0)
        return monitor();

    eprintln("no command selected");
    return 1;
}

} // namespace uni::cli
