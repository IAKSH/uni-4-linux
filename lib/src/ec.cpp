#include <uni/ec.hpp>

#include <cctype>
#include <charconv>
#include <chrono>
#include <format>
#include <system_error>
#include <thread>

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
    std::fstream file{std::string(path), std::ios::in | std::ios::out};
    if (!file)
        return std::unexpected(EcError::CannotOpen);
    return AcpiEc{std::move(file)};
}

std::expected<std::string, uni::EcError>
uni::AcpiEc::evaluate(std::string_view method) noexcept {
    if (!file_.is_open())
        return std::unexpected(EcError::CannotOpen);

    file_.clear();
    file_.seekp(0);
    file_ << method << '\n' << std::flush;
    if (!file_)
        return std::unexpected(EcError::WriteFailed);

    std::this_thread::sleep_for(kAcpiSettle);

    file_.clear();
    file_.seekg(0);
    std::string line;
    std::getline(file_, line);
    if (!file_ && !file_.eof())
        return std::unexpected(EcError::ReadFailed);
    return line;
}

std::expected<std::uint8_t, uni::EcError>
uni::AcpiEc::read(std::uint16_t addr) noexcept {
    const auto method = std::format("\\_SB.INOU.ECRR 0x{:x}", addr);
    auto result = evaluate(method);
    if (!result)
        return std::unexpected(result.error());
    return parse_ec_value(*result);
}

std::expected<void, uni::EcError>
uni::AcpiEc::write(std::uint16_t addr, std::uint8_t value) noexcept {
    const auto method = std::format("\\_SB.INOU.ECRW 0x{:x} 0x{:x}", addr,
                                    static_cast<unsigned>(value));
    auto result = evaluate(method);
    if (!result)
        return std::unexpected(result.error());

    const auto text = trim(*result);
    if (text.starts_with("Error") || text.starts_with("not supported"))
        return std::unexpected(EcError::DeviceError);
    return {};
}
