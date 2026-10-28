#pragma once

#include "model.hpp"

#include <ftxui/dom/elements.hpp>

#include <string_view>

namespace uni::tui {

ftxui::Element render(const Snapshot &snapshot, std::string_view status,
                      bool status_error);

} // namespace uni::tui
