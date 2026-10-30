#pragma once

#include <concepts>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace uni {

inline constexpr std::string_view kAcpiCallPath{"/proc/acpi/call"};
inline constexpr std::uint8_t kRgbMax{50};

enum class EcError {
    CannotOpen,
    WriteFailed,
    ReadFailed,
    DeviceError,
};

std::string_view describe(EcError error) noexcept;

// Uniwill EC register map, verified on the MECHREVO Jiguang Pro (GM5AG0O).
enum class Reg : std::uint16_t {
    ProjectId = 0x0740,
    KeyboardId = 0x073c,
    KbSupport = 0x0766,
    KbColorTrigger = 0x0767,
    KbColorR = 0x0769,
    KbColorG = 0x076a,
    KbColorB = 0x076b,
    KbEnable = 0x078c,
    KbPowerPrep = 0x07c5,

    FanMode = 0x0751,
    FanLeftDuty = 0x075b,
    FanRightDuty = 0x075c,
    FanLeftRpmHi = 0x0464,
    FanLeftRpmLo = 0x0465,
    FanRightRpmHi = 0x046c,
    FanRightRpmLo = 0x046b,
};

constexpr std::uint16_t reg(Reg value) noexcept {
    return static_cast<std::uint16_t>(value);
}

inline constexpr std::uint8_t kKbTriggerRgb{0x20};
inline constexpr std::uint8_t kKbTriggerRainbow{0x80};
inline constexpr std::uint8_t kKbPowerOff{0x02};
inline constexpr std::uint8_t kKbBrightnessApply{0x10};
inline constexpr std::uint8_t kFanBoost{0x40};
inline constexpr std::uint8_t kFanManual{0x80};

// Parse the textual result of an acpi_call invocation ("0x32", "50", ...).
std::expected<std::uint8_t, EcError> parse_ec_value(std::string_view text) noexcept;

// The only production EC backend: \_SB.INOU.ECRR / ECRW through acpi_call.
class AcpiEc {
public:
    static std::expected<AcpiEc, EcError> open(std::string_view path = kAcpiCallPath);

    AcpiEc(const AcpiEc &) = delete;
    AcpiEc &operator=(const AcpiEc &) = delete;
    AcpiEc(AcpiEc &&other) noexcept;
    AcpiEc &operator=(AcpiEc &&other) noexcept;
    ~AcpiEc();

    [[nodiscard]] std::expected<std::uint8_t, EcError> read(std::uint16_t addr) noexcept;
    [[nodiscard]] std::expected<void, EcError> write(std::uint16_t addr, std::uint8_t value) noexcept;

private:
    explicit AcpiEc(int fd) noexcept : fd_(fd) {}

    [[nodiscard]] std::expected<void, EcError> send(std::string_view method) noexcept;
    [[nodiscard]] std::expected<std::string, EcError> receive() noexcept;

    int fd_{-1};
};

template <typename T>
concept EcBackend = requires(T &ec, std::uint16_t addr, std::uint8_t value) {
    { ec.read(addr) } -> std::same_as<std::expected<std::uint8_t, EcError>>;
    { ec.write(addr, value) } -> std::same_as<std::expected<void, EcError>>;
};

static_assert(EcBackend<AcpiEc>);

// Thin read/modify/write layer.  The backend is a template parameter only so
// the unit tests can substitute an in-memory EC; production has one backend.
template <EcBackend Backend>
class Ec {
public:
    explicit Ec(Backend backend) : backend_(std::move(backend)) {}

    Ec(const Ec &) = delete;
    Ec &operator=(const Ec &) = delete;
    Ec(Ec &&) = default;
    Ec &operator=(Ec &&) = default;
    ~Ec() = default;

    [[nodiscard]] std::expected<std::uint8_t, EcError> read(std::uint16_t addr) {
        return backend_.read(addr);
    }

    [[nodiscard]] std::expected<void, EcError> write(std::uint16_t addr, std::uint8_t value) {
        return backend_.write(addr, value);
    }

    [[nodiscard]] std::expected<void, EcError> update(std::uint16_t addr, std::uint8_t mask,
                                                      std::uint8_t value) {
        auto old = backend_.read(addr);
        if (!old)
            return std::unexpected(old.error());
        const auto next = static_cast<std::uint8_t>(
            (*old & static_cast<std::uint8_t>(~mask)) | (value & mask));
        return backend_.write(addr, next);
    }

    [[nodiscard]] std::expected<void, EcError> rewrite(std::uint16_t addr) {
        auto value = backend_.read(addr);
        if (!value)
            return std::unexpected(value.error());
        return backend_.write(addr, *value);
    }

    [[nodiscard]] Backend &backend() noexcept { return backend_; }
    [[nodiscard]] const Backend &backend() const noexcept { return backend_; }

private:
    Backend backend_;
};

} // namespace uni
