#pragma once

#include <uni/ec.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace uni {

enum class MachineStatus {
    Target,
    LikelyTarget,
    Unknown,
    EcMismatch,
    Mismatch,
};

struct MachineInfo {
    std::string product;
    std::string board;
    std::string vendor;
};

struct EcIdentity {
    std::uint8_t project_id{};
    std::uint8_t keyboard_id{};
};

struct MachineCheck {
    MachineStatus status{MachineStatus::Unknown};
    MachineInfo info;
    std::optional<EcIdentity> ec;

    [[nodiscard]] bool writes_allowed() const noexcept {
        return status == MachineStatus::Target;
    }

    [[nodiscard]] std::string summary() const;
};

std::string_view describe(MachineStatus status) noexcept;

MachineInfo read_machine_info();

MachineCheck check_machine(const MachineInfo &info,
                           const std::optional<EcIdentity> &ec);

// Reads the EC fingerprint and combines it with DMI.  The EC reads are safe
// (ProjectId/KeyboardId only).
template <EcBackend EcT>
MachineCheck detect_machine(EcT &ec) {
    std::optional<EcIdentity> identity;
    const auto project = ec.read(reg(Reg::ProjectId));
    const auto keyboard = ec.read(reg(Reg::KeyboardId));
    if (project && keyboard)
        identity = EcIdentity{*project, *keyboard};
    return check_machine(read_machine_info(), identity);
}

} // namespace uni
