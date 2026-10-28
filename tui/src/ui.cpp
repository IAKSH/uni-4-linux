#include "ui.hpp"

#include <ftxui/ftxui.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <format>
#include <string>
#include <string_view>

namespace uni::tui {
namespace {

std::string clock_text(std::chrono::system_clock::time_point point) {
    if (point == std::chrono::system_clock::time_point{})
        return "--:--:--";

    const std::time_t time = std::chrono::system_clock::to_time_t(point);
    std::tm local{};
    localtime_r(&time, &local);
    return std::format("{:02}:{:02}:{:02}", local.tm_hour, local.tm_min, local.tm_sec);
}

std::string_view type_name(kbd::KbdType type) {
    switch (type) {
    case kbd::KbdType::OneZoneRgb:
        return "1-zone RGB";
    case kbd::KbdType::FixedColor:
        return "fixed colour";
    case kbd::KbdType::None:
        return "unknown";
    }
    return "unknown";
}

ftxui::Color swatch_color(const kbd::Rgb &levels) {
    const auto channel = [](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::min(255, static_cast<int>(value) * 5));
    };
    return ftxui::Color::RGB(channel(levels.r), channel(levels.g), channel(levels.b));
}

ftxui::Element bar(std::string_view label, float fraction, ftxui::Color color) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    return ftxui::hbox({
        ftxui::dim(ftxui::text(std::format("{:>10}", label))),
        ftxui::xflex(ftxui::gauge(fraction) | ftxui::color(color)),
        ftxui::text(std::format(" {:3.0f}%", fraction * 100.0f)),
    });
}

ftxui::Element hint(std::string_view key, std::string_view description) {
    return ftxui::hbox({
        ftxui::bgcolor(ftxui::Color::Blue,
                       ftxui::bold(ftxui::text(std::format(" {} ", key)))),
        ftxui::text(std::format(" {} ", description)),
    });
}

ftxui::Element keyboard_panel(const Snapshot &snapshot) {
    using namespace ftxui;
    const auto title = color(Color::Cyan, bold(text(" Keyboard ")));
    const auto &keyboard = snapshot.keyboard;

    if (!keyboard.available) {
        const auto &error =
            !keyboard.error.empty() ? keyboard.error : snapshot.ec_error;
        return window(title, color(Color::Red,
                                   text(error.empty() ? "unavailable" : error)));
    }

    Elements rows;
    rows.push_back(hbox({
        text("colour "),
        bgcolor(swatch_color(keyboard.levels), text("      ")),
        filler(),
        text(std::format("R{:02} G{:02} B{:02}",
                         static_cast<unsigned>(keyboard.levels.r),
                         static_cast<unsigned>(keyboard.levels.g),
                         static_cast<unsigned>(keyboard.levels.b))),
    }));
    rows.push_back(bar("red", keyboard.levels.r / 50.0f, Color::Red));
    rows.push_back(bar("green", keyboard.levels.g / 50.0f, Color::Green));
    rows.push_back(bar("blue", keyboard.levels.b / 50.0f, Color::Blue));
    rows.push_back(bar("brightness", keyboard.brightness / 4.0f, Color::Yellow));
    rows.push_back(hbox({
        text("power "),
        color(keyboard.power ? Color::Green : Color::Red,
              bold(text(keyboard.power ? "ON" : "OFF"))),
        text("   effect "),
        color(keyboard.rainbow ? Color::Magenta : Color::GrayLight,
              text(keyboard.rainbow ? "rainbow" : "static")),
    }));
    if (!keyboard.error.empty())
        rows.push_back(color(Color::Red, text(keyboard.error)));

    return window(title, vbox(std::move(rows)));
}

ftxui::Element fan_panel(const Snapshot &snapshot) {
    using namespace ftxui;
    const auto title = color(Color::Cyan, bold(text(" Fan ")));
    const auto &fan = snapshot.fan;

    if (!fan.available) {
        const auto &error = !fan.error.empty() ? fan.error : snapshot.ec_error;
        return window(title, color(Color::Red,
                                   text(error.empty() ? "unavailable" : error)));
    }

    const bool boost = (fan.mode & kFanBoost) != 0;
    const bool manual = (fan.mode & kFanManual) != 0;
    Elements rows;
    rows.push_back(hbox({
        text("mode "),
        color(boost ? Color::Red : Color::Green,
              bold(text(boost ? "BOOST" : "AUTO"))),
        text(manual ? "  manual" : "  automatic"),
        filler(),
        text(std::format("0x{:02x}", static_cast<unsigned>(fan.mode))),
    }));
    rows.push_back(bar("left duty", fan.left_duty / 200.0f, Color::Cyan));
    rows.push_back(bar("right duty", fan.right_duty / 200.0f, Color::Blue));
    rows.push_back(bar("left rpm",
                       std::min(1.0f, fan.left_rpm / 6000.0f), Color::Green));
    rows.push_back(bar("right rpm",
                       std::min(1.0f, fan.right_rpm / 6000.0f), Color::Magenta));
    rows.push_back(hbox({
        text(std::format("{} rpm  /  {} rpm", fan.left_rpm, fan.right_rpm)),
    }));
    if (!fan.error.empty())
        rows.push_back(color(Color::Red, text(fan.error)));

    return window(title, vbox(std::move(rows)));
}

ftxui::Element touchpad_panel(const Snapshot &snapshot) {
    using namespace ftxui;
    const auto title = color(Color::Cyan, bold(text(" Touchpad ")));
    const auto &touchpad = snapshot.touchpad;

    if (!touchpad.available) {
        return window(title,
                      color(Color::Red, text(touchpad.error.empty()
                                                 ? "unavailable"
                                                 : touchpad.error)));
    }

    return window(title, hbox({
                              text("state "),
                              color(touchpad.enabled ? Color::Green : Color::Red,
                                    bold(text(touchpad.enabled ? "enabled"
                                                               : "disabled"))),
                              filler(),
                              text(" t toggle "),
                          }));
}

ftxui::Element ec_panel(const Snapshot &snapshot) {
    using namespace ftxui;
    const auto title = color(Color::Cyan, bold(text(" EC ")));

    Elements rows;
    if (!snapshot.ec_error.empty()) {
        rows.push_back(color(Color::Red, text(snapshot.ec_error)));
    } else {
        rows.push_back(text(std::format("project   0x{:02x}",
                                        static_cast<unsigned>(
                                            snapshot.keyboard.project_id))));
        rows.push_back(text(std::format("keyboard  0x{:02x}",
                                        static_cast<unsigned>(
                                            snapshot.keyboard.keyboard_id))));
        rows.push_back(text(std::format("type      {}",
                                        type_name(snapshot.keyboard.type))));
    }
    rows.push_back(text(std::format("updated   {}", clock_text(snapshot.updated))));
    return window(title, vbox(std::move(rows)));
}

ftxui::Element status_line(std::string_view status, bool error) {
    using namespace ftxui;
    return hbox({
        text(" status "),
        color(error ? Color::Red : Color::Green,
              bold(text(status.empty() ? "ready" : status))),
    });
}

ftxui::Element footer() {
    using namespace ftxui;
    return hflow({
        hint("q", "quit"),
        hint("r", "refresh"),
        hint("c", "colour"),
        hint("b/B", "brightness"),
        hint("p", "power"),
        hint("e", "effect"),
        hint("f", "fan boost"),
        hint("t", "touchpad"),
    });
}

} // namespace

ftxui::Element render(const Snapshot &snapshot, std::string_view status,
                      bool status_error) {
    using namespace ftxui;

    const auto header = hbox({
        bgcolor(Color::Blue, bold(text(" uni-4-linux "))),
        color(Color::Cyan, bold(text(" GM5AG0O "))),
        filler(),
        color(Color::GrayLight, text(std::format(" {} ", clock_text(snapshot.updated)))),
    });

    const auto body = vbox({
        header,
        flex(hbox({flex(keyboard_panel(snapshot)), flex(fan_panel(snapshot))})),
        hbox({flex(touchpad_panel(snapshot)), flex(ec_panel(snapshot))}),
        separator(),
        status_line(status, status_error),
        footer(),
    });

    return bgcolor(Color::RGB(10, 12, 20), border(body));
}

} // namespace uni::tui
