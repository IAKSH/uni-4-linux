#include "input.hpp"

#include "touchpad.hpp"
#include "util.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct EventDevice {
    int fd{-1};
    std::string path;
    std::string name;
    bool meta{};
    bool ctrl{};

    EventDevice(int descriptor, std::string device_path, std::string device_name)
        : fd(descriptor), path(std::move(device_path)), name(std::move(device_name)) {}

    ~EventDevice() {
        if (fd >= 0)
            ::close(fd);
    }

    EventDevice(const EventDevice &) = delete;
    EventDevice &operator=(const EventDevice &) = delete;

    EventDevice(EventDevice &&other) noexcept
        : fd(std::exchange(other.fd, -1)), path(std::move(other.path)),
          name(std::move(other.name)), meta(other.meta), ctrl(other.ctrl) {}

    EventDevice &operator=(EventDevice &&other) noexcept {
        if (this != &other) {
            if (fd >= 0)
                ::close(fd);
            fd = std::exchange(other.fd, -1);
            path = std::move(other.path);
            name = std::move(other.name);
            meta = other.meta;
            ctrl = other.ctrl;
        }
        return *this;
    }
};

std::string query_name(int fd, const std::filesystem::path &path) {
    std::array<char, 256> buffer{};
    if (::ioctl(fd, EVIOCGNAME(static_cast<int>(buffer.size() - 1)), buffer.data()) >= 0)
        return buffer.data();
    return path.filename().string();
}

std::vector<EventDevice> open_devices() {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator("/dev/input", error)) {
        const auto filename = entry.path().filename().string();
        if (filename.starts_with("event"))
            paths.push_back(entry.path());
    }
    if (error)
        return {};

    std::sort(paths.begin(), paths.end());

    std::vector<EventDevice> devices;
    devices.reserve(paths.size());
    for (const auto &path : paths) {
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        devices.emplace_back(fd, path.string(), query_name(fd, path));
    }
    return devices;
}

std::string key_name(std::uint16_t code) {
    switch (code) {
    case KEY_F21:
        return "F21";
    case KEY_F24:
        return "F24";
    case KEY_LEFTMETA:
        return "LEFTMETA";
    case KEY_LEFTCTRL:
        return "LEFTCTRL";
    case KEY_TOUCHPAD_TOGGLE:
        return "TOUCHPAD_TOGGLE";
    case KEY_TOUCHPAD_ON:
        return "TOUCHPAD_ON";
    case KEY_TOUCHPAD_OFF:
        return "TOUCHPAD_OFF";
    case KEY_KBDILLUMTOGGLE:
        return "KBDILLUMTOGGLE";
    case KEY_KBDILLUMUP:
        return "KBDILLUMUP";
    case KEY_KBDILLUMDOWN:
        return "KBDILLUMDOWN";
    case BTN_LEFT:
        return "BTN_LEFT";
    case BTN_RIGHT:
        return "BTN_RIGHT";
    case BTN_TOUCH:
        return "BTN_TOUCH";
    case BTN_TOOL_FINGER:
        return "BTN_TOOL_FINGER";
    default:
        return std::format("KEY_{}", code);
    }
}

template <typename Handler>
std::expected<void, uni4linux::input::InputError>
poll_loop(std::vector<EventDevice> &devices, Handler &&handler) {
    std::vector<pollfd> fds;
    fds.reserve(devices.size());
    for (const auto &device : devices)
        fds.push_back(pollfd{device.fd, POLLIN, 0});

    for (;;) {
        const int ready = ::poll(fds.data(), fds.size(), -1);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            return std::unexpected(uni4linux::input::InputError::OpenFailed);
        }

        for (std::size_t i = 0; i < devices.size(); ++i) {
            if (fds[i].fd < 0)
                continue;

            if ((fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                ::close(devices[i].fd);
                devices[i].fd = -1;
                fds[i].fd = -1;
                continue;
            }
            if ((fds[i].revents & POLLIN) == 0)
                continue;

            input_event event{};
            while (::read(devices[i].fd, &event, sizeof(event)) ==
                   static_cast<ssize_t>(sizeof(event))) {
                handler(devices[i], event);
            }
        }
    }
}

int find_at_keyboard(std::string &path) {
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator("/dev/input", error)) {
        const auto filename = entry.path().filename().string();
        if (!filename.starts_with("event"))
            continue;

        const int fd = ::open(entry.path().c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0)
            continue;

        std::array<char, 256> name{};
        if (::ioctl(fd, EVIOCGNAME(static_cast<int>(name.size() - 1)), name.data()) >= 0 &&
            std::string_view(name.data()).find("AT Translated") != std::string_view::npos) {
            path = entry.path().string();
            return fd;
        }
        ::close(fd);
    }
    return -1;
}

} // namespace

namespace uni4linux::input {

std::string_view describe(InputError error) noexcept {
    switch (error) {
    case InputError::NoDevices:
        return "no readable /dev/input/event* devices";
    case InputError::OpenFailed:
        return "failed to poll input devices";
    case InputError::KeyboardNotFound:
        return "AT keyboard not found";
    case InputError::KeymapFailed:
        return "EVIOCSKEYCODE failed";
    }
    return "unknown input error";
}

std::expected<void, InputError> watch_touchpad() {
    auto devices = open_devices();
    if (devices.empty())
        return std::unexpected(InputError::NoDevices);

    eprintln("watching {} input device(s); toggle = Fn+F5 (Meta+Ctrl+F24) or KEY_F21",
             devices.size());
    for (const auto &device : devices)
        eprintln("  {}: {}", device.path, device.name);

    return poll_loop(devices, [](EventDevice &device, const input_event &event) {
        if (event.type != EV_KEY)
            return;
        if (event.code == KEY_LEFTMETA) {
            device.meta = event.value != 0;
            return;
        }
        if (event.code == KEY_LEFTCTRL) {
            device.ctrl = event.value != 0;
            return;
        }

        // 350 is the keycode used by the old `keymap f8 350` workaround.
        constexpr std::uint16_t kLegacyTouchpadKey = 350;
        const bool toggle =
            event.value == 1 &&
            (event.code == KEY_F21 || event.code == kLegacyTouchpadKey ||
             event.code == KEY_TOUCHPAD_TOGGLE ||
             (event.code == KEY_F24 && device.meta && device.ctrl));
        if (!toggle)
            return;

        auto state = touchpad::toggle();
        if (!state) {
            eprintln("touchpad: {}", touchpad::describe(state.error()));
            return;
        }
        println("touchpad {}", *state ? "enabled" : "disabled");
    });
}

std::expected<void, InputError> monitor() {
    auto devices = open_devices();
    if (devices.empty())
        return std::unexpected(InputError::NoDevices);

    eprintln("monitoring {} input device(s), press keys, Ctrl-C to stop:", devices.size());
    for (const auto &device : devices)
        eprintln("  {}: {}", device.path, device.name);

    return poll_loop(devices, [](const EventDevice &device, const input_event &event) {
        if (event.type != EV_KEY)
            return;
        println("{} {} {} ({})", device.path, key_name(event.code),
                event.value == 0 ? "release" : event.value == 1 ? "press" : "repeat",
                event.code);
    });
}

std::expected<void, InputError> remap_key(std::uint16_t scancode,
                                          std::uint16_t keycode) {
    std::string path;
    const int fd = find_at_keyboard(path);
    if (fd < 0)
        return std::unexpected(InputError::KeyboardNotFound);

    eprintln("keymap target: {}", path);

    input_keymap_entry entry{};
    entry.len = 2;
    entry.keycode = keycode;
    entry.scancode[0] = static_cast<std::uint8_t>(scancode & 0xff);
    entry.scancode[1] = static_cast<std::uint8_t>((scancode >> 8) & 0xff);

    if (::ioctl(fd, EVIOCSKEYCODE_V2, &entry) < 0) {
        const unsigned int codes[2] = {scancode, keycode};
        if (::ioctl(fd, EVIOCSKEYCODE, codes) < 0) {
            ::close(fd);
            return std::unexpected(InputError::KeymapFailed);
        }
    }
    ::close(fd);

    println("mapped scancode 0x{:x} -> keycode {} on {}", scancode, keycode, path);
    return {};
}

} // namespace uni4linux::input
