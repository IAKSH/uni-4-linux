#include "model.hpp"
#include "ui.hpp"

#include <ftxui/ftxui.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#ifndef UNI_VERSION
#define UNI_VERSION "0.1.0"
#endif

namespace {

void print_help() {
    std::cout << "uni-tui - btop-style dashboard for uni-4-linux\n"
                 "\n"
                 "Usage: uni-tui [--help|--version]\n"
                 "\n"
                 "Keys:\n"
                 "  q       quit\n"
                 "  r       refresh now\n"
                 "  c       cycle colour presets\n"
                 "  b / B   brightness up / down\n"
                 "  p       toggle keyboard power\n"
                 "  e       toggle firmware rainbow\n"
                 "  f       toggle fan boost\n"
                 "  t       toggle touchpad\n"
                 "\n"
                 "Mouse:\n"
                 "  drag the red/green/blue/brightness bars, release to apply\n";
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 1) {
        const std::string_view arg{argv[1]};
        if (arg == "--version") {
            std::cout << "uni-tui " UNI_VERSION "\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }
        print_help();
        return 2;
    }

    uni::tui::Model model;
    uni::tui::KeyboardEditor editor;
    model.refresh();
    editor.sync(model.snapshot().keyboard);

    auto screen = ftxui::App::Fullscreen();
    screen.TrackMouse(true);

    auto renderer = ftxui::Renderer([&model, &editor] {
        return uni::tui::render(model.snapshot(), model.status(),
                                model.status_error(), editor);
    });

    auto refresh = [&model, &editor] {
        model.refresh();
        editor.sync(model.snapshot().keyboard);
    };

    auto component = ftxui::CatchEvent(
        renderer, [&model, &editor, &screen, &refresh](ftxui::Event event) {
            using ftxui::Event;

            if (event.is_mouse()) {
                uni::tui::EditTarget target{};
                if (!editor.handle_mouse(event.mouse(), target))
                    return false;
                switch (target) {
                case uni::tui::EditTarget::Color:
                    model.set_color(editor.value);
                    break;
                case uni::tui::EditTarget::Brightness:
                    model.set_brightness(editor.brightness);
                    break;
                case uni::tui::EditTarget::None:
                    break;
                }
                editor.sync(model.snapshot().keyboard);
                return true;
            }

            if (event == Event::Custom) {
                refresh();
                return true;
            }
            if (event == Event::Character('q') || event == Event::Escape ||
                event == Event::CtrlC) {
                screen.Exit();
                return true;
            }
            if (event == Event::Character('r')) {
                refresh();
                return true;
            }
            if (event == Event::Character('c')) {
                model.cycle_color();
                editor.dirty = false;
                editor.dragging = -1;
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            if (event == Event::Character('b')) {
                model.adjust_brightness(1);
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            if (event == Event::Character('B')) {
                model.adjust_brightness(-1);
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            if (event == Event::Character('p')) {
                model.toggle_power();
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            if (event == Event::Character('e')) {
                model.toggle_effect();
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            if (event == Event::Character('f')) {
                model.toggle_fan_boost();
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            if (event == Event::Character('t')) {
                model.toggle_touchpad();
                editor.sync(model.snapshot().keyboard);
                return true;
            }
            return false;
        });

    std::atomic<bool> running{true};
    std::thread refresher([&screen, &running] {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (running.load())
                screen.PostEvent(ftxui::Event::Custom);
        }
    });

    screen.Loop(component);
    running = false;
    refresher.join();
    return 0;
}
