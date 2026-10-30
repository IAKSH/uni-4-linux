#include <uni/ec.hpp>

#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <fcntl.h>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <utility>

namespace {

constexpr auto kAcpiSettle = std::chrono::milliseconds(6);

std::string_view trim(std::string_view text) noexcept {
    const auto is_space = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };
    while (!text.empty() && is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && is_space(text.back()))
        text.remove_suffix(1);
    return text;
}

} // namespace

std::string_view uni::describe(EcError error) noexcept {
    switch (error) {
    case EcError::CannotOpen:
        return "cannot open /proc/acpi/call (load acpi_call and run as root)";
    case EcError::WriteFailed:
        return "failed to write the ACPI call";
    case EcError::ReadFailed:
        return "failed to read the ACPI call result";
    case EcError::DeviceError:
        return "the ACPI EC method returned an error";
    }
    return "unknown EC error";
}

std::expected<std::uint8_t, uni::EcError>
uni::parse_ec_value(std::string_view text) noexcept {
    // acpi_call returns a NUL-terminated buffer without a newline.
    if (const auto nul = text.find('\0'); nul != std::string_view::npos)
        text = text.substr(0, nul);
    text = trim(text);
    if (text.empty())
        return std::unexpected(EcError::ReadFailed);
    if (text.starts_with("Error") || text.starts_with("not supported"))
        return std::unexpected(EcError::DeviceError);

    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty())
        return std::unexpected(EcError::ReadFailed);

    unsigned value = 0;
    const auto *begin = text.data();
    const auto *end = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value, base);
    if (ec != std::errc{} || ptr != end || value > 0xff)
        return std::unexpected(EcError::ReadFailed);

    return static_cast<std::uint8_t>(value);
}

std::expected<uni::AcpiEc, uni::EcError>
uni::AcpiEc::open(std::string_view path) {
    const int fd = ::open(std::string(path).c_str(), O_RDWR);
    if (fd < 0)
        return std::unexpected(EcError::CannotOpen);
    return AcpiEc{fd};
}

uni::AcpiEc::AcpiEc(AcpiEc &&other) noexcept
    : fd_(std::exchange(other.fd_, -1)) {}

uni::AcpiEc &uni::AcpiEc::operator=(AcpiEc &&other) noexcept {
    if (this != &other) {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
}

uni::AcpiEc::~AcpiEc() {
    if (fd_ >= 0)
        ::close(fd_);
}

std::expected<void, uni::EcError>
uni::AcpiEc::send(std::string_view method) {
    if (fd_ < 0)
        return std::unexpected(EcError::CannotOpen);
    if (::lseek(fd_, 0, SEEK_SET) < 0)
        return std::unexpected(EcError::WriteFailed);

    const auto command = std::format("{}\n", method);
    const auto written = ::write(fd_, command.data(), command.size());
    if (written != static_cast<ssize_t>(command.size()))
        return std::unexpected(EcError::WriteFailed);
    return {};
}

std::expected<std::string, uni::EcError> uni::AcpiEc::receive() {
    if (fd_ < 0)
        return std::unexpected(EcError::CannotOpen);
    if (::lseek(fd_, 0, SEEK_SET) < 0)
        return std::unexpected(EcError::ReadFailed);

    std::array<char, 128> buffer{};
    const auto count = ::read(fd_, buffer.data(), buffer.size() - 1);
    if (count <= 0)
        return std::unexpected(EcError::ReadFailed);

    const auto size = static_cast<std::size_t>(count);
    buffer[size] = '\0';
    const std::string_view text(buffer.data());
    if (text.find("Error") != std::string_view::npos ||
        text.find("not supported") != std::string_view::npos)
        return std::unexpected(EcError::DeviceError);

    return std::string(text);
}

std::expected<std::uint8_t, uni::EcError>
uni::AcpiEc::read(std::uint16_t addr) {
    const auto method = std::format("\\_SB.INOU.ECRR 0x{:x}", addr);
    if (auto sent = send(method); !sent)
        return std::unexpected(sent.error());

    std::this_thread::sleep_for(kAcpiSettle);

    auto result = receive();
    if (!result)
        return std::unexpected(result.error());
    return parse_ec_value(*result);
}

std::expected<void, uni::EcError>
uni::AcpiEc::write(std::uint16_t addr, std::uint8_t value) {
    const auto method = std::format("\\_SB.INOU.ECRW 0x{:x} 0x{:x}", addr,
                                    static_cast<unsigned>(value));
    if (auto sent = send(method); !sent)
        return std::unexpected(sent.error());

    // ECRW is void; the original implementation never reads a result here.
    std::this_thread::sleep_for(kAcpiSettle);
    return {};
}
