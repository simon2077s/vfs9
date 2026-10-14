/**
 * @file exit.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `exit` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/exit.hpp"

#include "vfs9_cli/cli.hpp"

namespace vfs9_cli {

bool exit_command::run(cli& context, const std::vector<std::string>& args) {
  (void)context;
  (void)args;
  return false;
}

} // namespace vfs9_cli
