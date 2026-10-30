#include <uni/machine.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::string trim(std::string_view text) {
    const auto is_space = [](char c) {
        return std::isspace(static_cast<unsigned char>(c)) != 0;
    };
    while (!text.empty() && is_space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && is_space(text.back()))
        text.remove_suffix(1);
    return std::string(text);
}

std::string read_dmi(std::string_view name) {
    std::ifstream file(std::format("/sys/class/dmi/id/{}", name));
    if (!file)
        return {};

    std::string line;
    std::getline(file, line);
    return trim(line);
}

bool contains(std::string_view text, std::string_view needle) {
    return text.find(needle) != std::string_view::npos;
}

} // namespace

std::string uni::MachineCheck::summary() const {
    std::string ec_text = "EC unavailable";
    if (ec) {
        ec_text = std::format("EC project 0x{:02x} keyboard 0x{:02x}",
                              static_cast<unsigned>(ec->project_id),
                              static_cast<unsigned>(ec->keyboard_id));
    }

    return std::format(
        "expected DMI GM5AG0O / EC project 0x10 keyboard 0x01; "
        "detected product='{}' board='{}' vendor='{}' {}",
        info.product, info.board, info.vendor, ec_text);
}

std::string_view uni::describe(MachineStatus status) noexcept {
    switch (status) {
    case MachineStatus::Target:
        return "target machine";
    case MachineStatus::LikelyTarget:
        return "likely target (DMI mismatch, EC fingerprint matches)";
    case MachineStatus::Unknown:
        return "unknown machine (no DMI/EC identity)";
    case MachineStatus::EcMismatch:
        return "DMI matches but EC fingerprint does not";
    case MachineStatus::Mismatch:
        return "not the target machine";
    }
    return "unknown machine";
}

uni::MachineInfo uni::read_machine_info() {
    MachineInfo info;
    info.product = read_dmi("product_name");
    info.board = read_dmi("board_name");

    const auto system_vendor = read_dmi("sys_vendor");
    info.vendor = !system_vendor.empty() ? system_vendor : read_dmi("board_vendor");
    return info;
}

uni::MachineCheck
uni::check_machine(const MachineInfo &info,
                   const std::optional<EcIdentity> &ec) {
    const bool dmi_target =
        contains(info.product, "GM5AG0O") || contains(info.board, "GM5AG0O");
    const bool dmi_known =
        !info.product.empty() || !info.board.empty() || !info.vendor.empty();
    const bool ec_target =
        ec && ec->project_id == 0x10 && ec->keyboard_id == 0x01;

    MachineStatus status = MachineStatus::Unknown;
    if (dmi_target && ec_target)
        status = MachineStatus::Target;
    else if (dmi_target)
        status = MachineStatus::EcMismatch;
    else if (ec_target)
        status = MachineStatus::LikelyTarget;
    else if (dmi_known)
        status = MachineStatus::Mismatch;

    return MachineCheck{status, info, ec};
}
