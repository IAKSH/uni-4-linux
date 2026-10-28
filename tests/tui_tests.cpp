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
    const auto element = uni::tui::render(snapshot, status, status_error);
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
