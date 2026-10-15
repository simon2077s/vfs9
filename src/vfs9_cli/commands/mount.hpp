/**
 * @file mount.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `mount` command
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
 * @brief Mount a file system
 *
 */
class mount_command final : public command {
public:
  std::string_view name() const noexcept override { return "mount"; }

  std::string_view usage() const noexcept override {
    return "mount fs_name:name[:args] old_path [-a][-b][-w]";
  }

  std::string_view description() const noexcept override {
    return "Mount a file system (mem, local, zip, pak) to a directory";
  }

  bool run(cli& context, const std::vector<std::string>& args) override;
};

} // namespace vfs9_cli
