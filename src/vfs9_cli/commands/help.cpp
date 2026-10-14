/**
 * @file help.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `help` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/help.hpp"

#include <iostream>

#include "vfs9_cli/cli.hpp"

namespace vfs9_cli {

bool help_command::run(cli& context, const std::vector<std::string>& args) {
  (void)args;
  std::cout << "available commands:\n";
  for (const auto& entry : context.commands()) {
    std::cout << "  " << entry.second->usage() << "\n";
    std::cout << "      " << entry.second->description() << "\n";
  }
  return true;
}

} // namespace vfs9_cli
