/**
 * @file bind.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `bind` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "vfs9_cli/command.hpp"

namespace vfs9_cli {

/**
 * @brief Bind a path to another path
 *
 */
class bind_command final : public command {
public:
  std::string_view name() const noexcept override { return "bind"; }

  std::string_view usage() const noexcept override { return "bind new_path old_path [-a][-b][-w]"; }

  std::string_view description() const noexcept override { return "Bind new_path onto old_path"; }

  bool run(cli& context, const std::vector<std::string>& args) override;
};

} // namespace vfs9_cli
