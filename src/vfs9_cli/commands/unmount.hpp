/**
 * @file unmount.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `unmount` command
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
 * @brief Unmount a mount or bind entry by name or source path
 *
 */
class unmount_command final : public command {
public:
  std::string_view name() const noexcept override { return "unmount"; }

  std::string_view usage() const noexcept override { return "unmount name/new_path old_path"; }

  std::string_view description() const noexcept override {
    return "Unmount the mount/bind entry identified by name or new_path from old_path";
  }

  bool run(cli& context, const std::vector<std::string>& args) override;
};

} // namespace vfs9_cli
