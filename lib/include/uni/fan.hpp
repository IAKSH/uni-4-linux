#pragma once

#include <uni/ec.hpp>

#include <cstdint>
#include <expected>

namespace uni::fan {

struct FanState {
    std::uint8_t mode{};
    std::uint8_t left_duty{};
    std::uint8_t right_duty{};
    std::uint16_t left_rpm{};
    std::uint16_t right_rpm{};
};

constexpr std::uint16_t combine_rpm(std::uint8_t hi, std::uint8_t lo) noexcept {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
}

template <EcBackend EcT>
class Fan {
public:
    explicit Fan(EcT &ec) : ec_(ec) {}

    [[nodiscard]] std::expected<FanState, EcError> state() {
        auto mode = ec_.read(reg(Reg::FanMode));
        if (!mode)
            return std::unexpected(mode.error());
        auto left_duty = ec_.read(reg(Reg::FanLeftDuty));
        if (!left_duty)
            return std::unexpected(left_duty.error());
        auto right_duty = ec_.read(reg(Reg::FanRightDuty));
        if (!right_duty)
            return std::unexpected(right_duty.error());
        auto left_hi = ec_.read(reg(Reg::FanLeftRpmHi));
        if (!left_hi)
            return std::unexpected(left_hi.error());
        auto left_lo = ec_.read(reg(Reg::FanLeftRpmLo));
        if (!left_lo)
            return std::unexpected(left_lo.error());
        auto right_hi = ec_.read(reg(Reg::FanRightRpmHi));
        if (!right_hi)
            return std::unexpected(right_hi.error());
        auto right_lo = ec_.read(reg(Reg::FanRightRpmLo));
        if (!right_lo)
            return std::unexpected(right_lo.error());

        return FanState{
            *mode,
            *left_duty,
            *right_duty,
            combine_rpm(*left_hi, *left_lo),
            combine_rpm(*right_hi, *right_lo),
        };
    }

    [[nodiscard]] std::expected<void, EcError> set_boost(bool on) {
        return ec_.update(reg(Reg::FanMode), kFanBoost, on ? kFanBoost : std::uint8_t{0});
    }

private:
    EcT &ec_;
};

} // namespace uni::fan
