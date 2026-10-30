#pragma once

#include <uni/ec.hpp>
#include <uni/kbd.hpp>
#include <uni/machine.hpp>

#include <charconv>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

#include "util.hpp"

namespace uni::cli {

enum class Access {
    Read,
    Write,
};

template <typename Fn>
int with_ec(Access access, bool force, Fn &&fn) {
    auto backend = AcpiEc::open();
    if (!backend) {
        eprintln("EC: {}", describe(backend.error()));
        return 1;
    }

    Ec ec{std::move(*backend)};
    const auto check = detect_machine(ec);
    if (check.status != MachineStatus::Target)
        eprintln("warning: {}: {}", describe(check.status), check.summary());

    if (access == Access::Write && !check.writes_allowed() && !force) {
        eprintln("EC: writes disabled because the target machine check failed");
        eprintln("EC: use --force to override (at your own risk)");
        return 1;
    }
    if (access == Access::Write && force && !check.writes_allowed())
        eprintln("warning: EC writes forced by --force");

    auto result = fn(ec);
    if (!result) {
        eprintln("EC: {}", describe(result.error()));
        return 1;
    }
    return 0;
}

template <typename Fn>
int with_ec(bool force, Fn &&fn) {
    return with_ec(Access::Write, force, std::forward<Fn>(fn));
}

template <typename Fn>
int with_ec(Fn &&fn) {
    return with_ec(Access::Read, false, std::forward<Fn>(fn));
}

template <typename T>
std::optional<T> parse_number(std::string_view text) {
    if (text.empty())
        return std::nullopt;

    int base = 10;
    if (text.starts_with("0x") || text.starts_with("0X")) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty())
        return std::nullopt;

    T value{};
    const auto *begin = text.data();
    const auto *end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, value, base);
    if (error != std::errc{} || ptr != end)
        return std::nullopt;
    return value;
}

inline std::optional<std::uint16_t> parse_scancode(std::string_view text) {
    if (text.starts_with("0x") || text.starts_with("0X"))
        text.remove_prefix(2);
    if (text.empty())
        return std::nullopt;

    std::uint16_t value{};
    const auto *begin = text.data();
    const auto *end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, value, 16);
    if (error != std::errc{} || ptr != end)
        return std::nullopt;
    return value;
}

inline std::string_view type_name(kbd::KbdType type) {
    switch (type) {
    case kbd::KbdType::OneZoneRgb:
        return "1-zone RGB";
    case kbd::KbdType::FixedColor:
        return "fixed colour";
    case kbd::KbdType::None:
        return "unknown/none";
    }
    return "unknown/none";
}

} // namespace uni::cli
