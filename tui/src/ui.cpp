#include "ui.hpp"

#include <ftxui/ftxui.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <format>
#include <string>
#include <string_view>
#include <utility>

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

ftxui::Color swatch_color(const kbd::Rgb &value) {
    return ftxui::Color::RGB(value.r, value.g, value.b);
}

ftxui::Element bar(std::string_view label, float fraction, ftxui::Color color,
                   ftxui::Box *box = nullptr) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    auto graph = ftxui::gauge(fraction) | ftxui::color(color);
    if (box != nullptr)
        graph = graph | ftxui::reflect(*box);
    return ftxui::hbox({
        ftxui::dim(ftxui::text(std::format("{:>10}", label))),
        ftxui::xflex(std::move(graph)),
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

ftxui::Element keyboard_panel(const Snapshot &snapshot, ColorEditor &editor) {
    using namespace ftxui;
    const auto title = color(Color::Cyan, bold(text(" Keyboard ")));
    const auto &keyboard = snapshot.keyboard;

    if (!keyboard.available) {
        editor.boxes = {};
        const auto &error =
            !keyboard.error.empty() ? keyboard.error : snapshot.ec_error;
        return window(title, color(Color::Red,
                                   text(error.empty() ? "unavailable" : error)));
    }

    Elements rows;
    rows.push_back(hbox({
        text("colour "),
        bgcolor(swatch_color(editor.value), text("      ")),
        filler(),
        text(std::format("R{:03} G{:03} B{:03}",
                         static_cast<unsigned>(editor.value.r),
                         static_cast<unsigned>(editor.value.g),
                         static_cast<unsigned>(editor.value.b))),
    }));
    rows.push_back(bar("red", editor.value.r / 255.0f, Color::Red,
                       &editor.boxes[0]));
    rows.push_back(bar("green", editor.value.g / 255.0f, Color::Green,
                       &editor.boxes[1]));
    rows.push_back(bar("blue", editor.value.b / 255.0f, Color::Blue,
                       &editor.boxes[2]));
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
        hint("drag", "RGB"),
        hint("b/B", "brightness"),
        hint("p", "power"),
        hint("e", "effect"),
        hint("f", "fan boost"),
        hint("t", "touchpad"),
    });
}

} // namespace

void ColorEditor::sync(const KeyboardSnapshot &keyboard) noexcept {
    if (dirty || !keyboard.available)
        return;

    const auto convert = [](std::uint8_t level) {
        return static_cast<std::uint8_t>(
            (static_cast<int>(level) * 255 + 25) / 50);
    };
    value = kbd::Rgb{convert(keyboard.levels.r), convert(keyboard.levels.g),
                     convert(keyboard.levels.b)};
}

int ColorEditor::channel_at(int x, int y) const noexcept {
    for (int index = 0; index < 3; ++index) {
        if (boxes[static_cast<std::size_t>(index)].Contain(x, y))
            return index;
    }
    return -1;
}

void ColorEditor::begin(int channel, int x) noexcept {
    if (channel < 0 || channel >= 3)
        return;
    dragging = channel;
    dirty = true;
    set_from_x(channel, x);
}

void ColorEditor::update(int x) noexcept {
    if (dragging >= 0)
        set_from_x(dragging, x);
}

bool ColorEditor::handle_mouse(const ftxui::Mouse &mouse, bool &commit) noexcept {
    using ftxui::Mouse;
    commit = false;

    if (mouse.motion == Mouse::Pressed && mouse.button == Mouse::Left) {
        const int channel = channel_at(mouse.x, mouse.y);
        if (channel >= 0) {
            begin(channel, mouse.x);
            return true;
        }
        return false;
    }

    if (mouse.motion == Mouse::Moved && dragging >= 0) {
        update(mouse.x);
        return true;
    }

    if (mouse.motion == Mouse::Released &&
        (dragging >= 0 || dirty)) {
        commit = true;
        dragging = -1;
        dirty = false;
        return true;
    }

    return false;
}

void ColorEditor::set_from_x(int channel, int x) noexcept {
    const auto &box = boxes[static_cast<std::size_t>(channel)];
    const int width = std::max(1, box.x_max - box.x_min);
    const float ratio = std::clamp(
        static_cast<float>(x - box.x_min) / static_cast<float>(width), 0.0f,
        1.0f);
    const auto value8 = static_cast<std::uint8_t>(std::lround(ratio * 255.0f));

    switch (channel) {
    case 0:
        value.r = value8;
        break;
    case 1:
        value.g = value8;
        break;
    case 2:
        value.b = value8;
        break;
    default:
        break;
    }
    dirty = true;
}

ftxui::Element render(const Snapshot &snapshot, std::string_view status,
                      bool status_error, ColorEditor &editor) {
    using namespace ftxui;

    const auto header = hbox({
        bgcolor(Color::Blue, bold(text(" uni-4-linux "))),
        color(Color::Cyan, bold(text(" GM5AG0O "))),
        filler(),
        color(Color::GrayLight, text(std::format(" {} ", clock_text(snapshot.updated)))),
    });

    const auto body = vbox({
        header,
        flex(hbox({flex(keyboard_panel(snapshot, editor)), flex(fan_panel(snapshot))})),
        hbox({flex(touchpad_panel(snapshot)), flex(ec_panel(snapshot))}),
        separator(),
        status_line(status, status_error),
        footer(),
    });

    return bgcolor(Color::RGB(10, 12, 20), border(body));
}

} // namespace uni::tui
