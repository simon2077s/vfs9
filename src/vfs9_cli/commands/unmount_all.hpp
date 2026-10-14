/**
 * @file unmount_all.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `unmount-all` command
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
 * @brief Unmount every entry of a path, or every entry of the namespace
 *
 */
class unmount_all_command final : public command {
public:
  std::string_view name() const noexcept override { return "unmount-all"; }

  std::string_view usage() const noexcept override { return "unmount-all [old_path]"; }

  std::string_view description() const noexcept override {
    return "Unmount all entries of old_path (or the whole namespace when omitted)";
  }

  bool run(cli& context, const std::vector<std::string>& args) override;
};

} // namespace vfs9_cli
