#pragma once

#include "model.hpp"

#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace uni::tui {

// Local RGB editor for the TUI.  Dragging updates only this state; the
// hardware is written when the mouse button is released.
struct ColorEditor {
    kbd::Rgb value{}; // 0..255 per channel
    bool dirty{};
    int dragging{-1};
    std::array<ftxui::Box, 3> boxes{};

    void sync(const KeyboardSnapshot &keyboard) noexcept;
    [[nodiscard]] int channel_at(int x, int y) const noexcept;
    void begin(int channel, int x) noexcept;
    void update(int x) noexcept;

    // Handles press/drag/release.  On release, commit is set to true after
    // the editor state is finalized.
    bool handle_mouse(const ftxui::Mouse &mouse, bool &commit) noexcept;

private:
    void set_from_x(int channel, int x) noexcept;
};

ftxui::Element render(const Snapshot &snapshot, std::string_view status,
                      bool status_error, ColorEditor &editor);

} // namespace uni::tui
