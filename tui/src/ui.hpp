#pragma once

#include "model.hpp"

#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace uni::tui {

enum class EditTarget {
    None,
    Color,
    Brightness,
};

// Local editor for the TUI.  Dragging updates only this state; the hardware
// is written when the mouse button is released.
struct KeyboardEditor {
    kbd::Rgb value{}; // 0..255 per channel
    int brightness{}; // 0..4
    bool dirty{};
    int dragging{-1}; // 0..2 = RGB, 3 = brightness
    std::array<ftxui::Box, 3> color_boxes{};
    ftxui::Box brightness_box{};

    void sync(const KeyboardSnapshot &keyboard) noexcept;
    [[nodiscard]] int channel_at(int x, int y) const noexcept;
    void begin(int channel, int x) noexcept;
    void update(int x) noexcept;

    // Handles press/drag/release.  On release, target reports what must be
    // written to the hardware.
    bool handle_mouse(const ftxui::Mouse &mouse, EditTarget &target) noexcept;

private:
    void set_from_x(int channel, int x) noexcept;
};

ftxui::Element render(const Snapshot &snapshot, std::string_view status,
                      bool status_error, KeyboardEditor &editor);

} // namespace uni::tui
