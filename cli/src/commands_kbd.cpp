#include "commands.hpp"
#include "support.hpp"
#include "util.hpp"

#include <uni/kbd.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <expected>
#include <thread>

namespace uni::cli {

int kbd_color(kbd::Rgb rgb, bool force) {
    return with_ec(force, [rgb](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        return keyboard.set_color(rgb);
    });
}

int kbd_brightness(int brightness, bool force) {
    return with_ec(force, [brightness](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        return keyboard.set_brightness(brightness);
    });
}

int kbd_power(bool on, bool force) {
    return with_ec(force, [on](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        return keyboard.set_power(on);
    });
}

int kbd_effect(bool rainbow, bool force) {
    return with_ec(force, [rainbow](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        return keyboard.set_effect(rainbow);
    });
}

int kbd_off(bool force) {
    return with_ec(force, [](auto &ec) -> std::expected<void, EcError> {
        kbd::Keyboard keyboard{ec};
        return keyboard.off();
    });
}

int kbd_breathe(kbd::Rgb rgb, int period_ms, bool force) {
    return with_ec(force, [rgb, period_ms](auto &ec) -> std::expected<void, EcError> {
        const kbd::Rgb levels{
            kbd::scale_rgb(rgb.r),
            kbd::scale_rgb(rgb.g),
            kbd::scale_rgb(rgb.b),
        };
        kbd::Keyboard keyboard{ec};
        const auto frame = std::chrono::milliseconds(std::max(1, period_ms / 100));
        eprintln("breathing colour {},{},{} period {}ms, Ctrl-C to stop",
                 static_cast<unsigned>(rgb.r), static_cast<unsigned>(rgb.g),
                 static_cast<unsigned>(rgb.b), period_ms);

        int level = 0;
        int direction = 1;
        for (;;) {
            const kbd::Rgb current{
                static_cast<std::uint8_t>(levels.r * level / kRgbMax),
                static_cast<std::uint8_t>(levels.g * level / kRgbMax),
                static_cast<std::uint8_t>(levels.b * level / kRgbMax),
            };
            auto result = keyboard.set_levels(current);
            if (!result)
                return std::unexpected(result.error());

            std::this_thread::sleep_for(frame);
            level += direction;
            if (level >= kRgbMax) {
                level = kRgbMax;
                direction = -1;
            } else if (level <= 0) {
                level = 0;
                direction = 1;
            }
        }
    });
}

} // namespace uni::cli
