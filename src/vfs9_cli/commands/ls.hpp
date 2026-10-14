/**
 * @file ls.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `ls` command
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
 * @brief List the entries of a directory
 *
 */
class ls_command final : public command {
public:
  std::string_view name() const noexcept override { return "ls"; }

  std::string_view usage() const noexcept override { return "ls [path]"; }

  std::string_view description() const noexcept override {
    return "List the entries of a directory (current directory when path is omitted)";
  }

  bool run(cli& context, const std::vector<std::string>& args) override;
};

} // namespace vfs9_cli
