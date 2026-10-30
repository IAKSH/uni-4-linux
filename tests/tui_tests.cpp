#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ui.hpp"

#include <ftxui/ftxui.hpp>

#include <chrono>
#include <string>

namespace {

uni::tui::Snapshot sample_snapshot() {
    uni::tui::Snapshot snapshot;
    snapshot.keyboard.available = true;
    snapshot.keyboard.project_id = 0x82;
    snapshot.keyboard.keyboard_id = 0x01;
    snapshot.keyboard.type = uni::kbd::KbdType::OneZoneRgb;
    snapshot.keyboard.levels = {50, 10, 0};
    snapshot.keyboard.brightness = 3;
    snapshot.keyboard.power = true;
    snapshot.keyboard.rainbow = false;

    snapshot.fan.available = true;
    snapshot.fan.mode = uni::kFanBoost | uni::kFanManual;
    snapshot.fan.left_duty = 120;
    snapshot.fan.right_duty = 100;
    snapshot.fan.left_rpm = 3200;
    snapshot.fan.right_rpm = 3000;

    snapshot.touchpad.available = true;
    snapshot.touchpad.enabled = true;
    snapshot.updated = std::chrono::system_clock::now();
    return snapshot;
}

std::string render_text(const uni::tui::Snapshot &snapshot,
                        std::string_view status, bool status_error) {
    uni::tui::KeyboardEditor editor;
    editor.sync(snapshot.keyboard);
    const auto element = uni::tui::render(snapshot, status, status_error, editor);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(100),
                                        ftxui::Dimension::Fixed(30));
    ftxui::Render(screen, element);
    return screen.ToString();
}

} // namespace

TEST_CASE("TUI renders panels and progress bars") {
    const auto text = render_text(sample_snapshot(), "test status", false);

    CHECK(text.find("Keyboard") != std::string::npos);
    CHECK(text.find("Fan") != std::string::npos);
    CHECK(text.find("Touchpad") != std::string::npos);
    CHECK(text.find("EC") != std::string::npos);
    CHECK(text.find("BOOST") != std::string::npos);
    CHECK(text.find("static") != std::string::npos);
    CHECK(text.find("test status") != std::string::npos);
    CHECK(text.find("%") != std::string::npos);
}

TEST_CASE("TUI renders hardware errors without crashing") {
    uni::tui::Snapshot snapshot;
    snapshot.ec_error = "no acpi_call";
    snapshot.touchpad.error = "touchpad not found";

    const auto text = render_text(snapshot, "hardware unavailable", true);
    CHECK(text.find("no acpi_call") != std::string::npos);
    CHECK(text.find("touchpad not found") != std::string::npos);
    CHECK(text.find("hardware unavailable") != std::string::npos);
}

TEST_CASE("TUI keyboard editor maps RGB drag positions") {
    uni::tui::KeyboardEditor editor;
    editor.color_boxes[0] = ftxui::Box{10, 110, 5, 5};
    editor.color_boxes[1] = ftxui::Box{10, 110, 6, 6};
    editor.color_boxes[2] = ftxui::Box{10, 110, 7, 7};
    editor.brightness_box = ftxui::Box{10, 110, 8, 8};

    CHECK(editor.channel_at(10, 5) == 0);
    CHECK(editor.channel_at(10, 6) == 1);
    CHECK(editor.channel_at(10, 7) == 2);
    CHECK(editor.channel_at(10, 8) == 3);
    CHECK(editor.channel_at(9, 5) == -1);

    editor.begin(0, 10);
    CHECK(editor.value.r == 0);
    CHECK(editor.dirty);
    editor.update(60);
    CHECK(editor.value.r == 128);
    editor.update(110);
    CHECK(editor.value.r == 255);

    editor.begin(1, 10);
    editor.update(110);
    CHECK(editor.value.g == 255);

    editor.begin(2, 60);
    CHECK(editor.value.b == 128);

    editor.begin(3, 60);
    CHECK(editor.brightness == 2);

    // A periodic refresh must not overwrite a drag in progress.
    uni::tui::Snapshot snapshot;
    snapshot.keyboard.available = true;
    snapshot.keyboard.levels = {50, 0, 0};
    editor.sync(snapshot.keyboard);
    CHECK(editor.value.b == 128);

    editor.dirty = false;
    editor.sync(snapshot.keyboard);
    CHECK(editor.value.r == 255);
    CHECK(editor.value.g == 0);
    CHECK(editor.value.b == 0);
    CHECK(editor.brightness == 0);
}

TEST_CASE("TUI keyboard editor tracks reflected RGB and brightness boxes") {
    auto snapshot = sample_snapshot();
    uni::tui::KeyboardEditor editor;
    editor.sync(snapshot.keyboard);

    const auto element = uni::tui::render(snapshot, "drag test", false, editor);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(100),
                                        ftxui::Dimension::Fixed(30));
    ftxui::Render(screen, element);

    for (const auto &box : editor.color_boxes)
        CHECK_FALSE(box.IsEmpty());
    CHECK_FALSE(editor.brightness_box.IsEmpty());

    const auto &red = editor.color_boxes[0];
    CHECK(editor.channel_at(red.x_min, red.y_min) == 0);
    CHECK(editor.channel_at(editor.brightness_box.x_min,
                            editor.brightness_box.y_min) == 3);

    editor.begin(0, red.x_min);
    CHECK(editor.value.r == 0);
    editor.update(red.x_max);
    CHECK(editor.value.r == 255);
    editor.dirty = false;
}

TEST_CASE("TUI keyboard editor handles RGB mouse press, drag, and release") {
    uni::tui::KeyboardEditor editor;
    editor.color_boxes[0] = ftxui::Box{10, 110, 5, 5};

    ftxui::Mouse mouse;
    mouse.button = ftxui::Mouse::Left;
    mouse.motion = ftxui::Mouse::Pressed;
    mouse.x = 10;
    mouse.y = 5;

    uni::tui::EditTarget target{};
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(target == uni::tui::EditTarget::None);
    CHECK(editor.dragging == 0);
    CHECK(editor.dirty);
    CHECK(editor.value.r == 0);

    mouse.motion = ftxui::Mouse::Moved;
    mouse.x = 110;
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(editor.value.r == 255);
    CHECK(target == uni::tui::EditTarget::None);

    mouse.motion = ftxui::Mouse::Released;
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(target == uni::tui::EditTarget::Color);
    CHECK(editor.dragging == -1);
    CHECK_FALSE(editor.dirty);
    CHECK(editor.value.r == 255);
}

TEST_CASE("TUI keyboard editor drags brightness 0..4") {
    uni::tui::KeyboardEditor editor;
    editor.brightness_box = ftxui::Box{10, 110, 8, 8};

    ftxui::Mouse mouse;
    mouse.button = ftxui::Mouse::Left;
    mouse.motion = ftxui::Mouse::Pressed;
    mouse.x = 10;
    mouse.y = 8;

    uni::tui::EditTarget target{};
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(editor.dragging == 3);
    CHECK(editor.brightness == 0);

    mouse.motion = ftxui::Mouse::Moved;
    mouse.x = 60;
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(editor.brightness == 2);

    mouse.x = 110;
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(editor.brightness == 4);

    mouse.motion = ftxui::Mouse::Released;
    CHECK(editor.handle_mouse(mouse, target));
    CHECK(target == uni::tui::EditTarget::Brightness);
    CHECK(editor.dragging == -1);
    CHECK_FALSE(editor.dirty);
    CHECK(editor.brightness == 4);
}
