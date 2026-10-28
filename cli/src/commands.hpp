#pragma once

#include <uni/kbd.hpp>

#include <cstdint>

namespace uni::cli {

int probe();

int kbd_color(kbd::Rgb rgb);
int kbd_brightness(int brightness);
int kbd_power(bool on);
int kbd_effect(bool rainbow);
int kbd_off();
int kbd_breathe(kbd::Rgb rgb, int period_ms);

int tp_get();
int tp_set(bool enable);
int tp_toggle();
int tp_watch();

int fan_get();
int fan_boost(bool on);

// Debug / raw access commands.
int ec_read(std::uint16_t addr);
int ec_write(std::uint16_t addr, std::uint8_t value);
int ec_dump(std::uint16_t addr, std::uint32_t length);
int kbd_test(kbd::Rgb rgb);
int keymap(std::uint16_t scancode, std::uint16_t keycode);
int monitor();

} // namespace uni::cli
