/**
 * @file pwd.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `pwd` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/pwd.hpp"

#include "vfs9_cli/cli.hpp"

namespace vfs9_cli {

bool pwd_command::run(cli& context, const std::vector<std::string>& args) {
  (void)args;
  // Use .string() because streaming a path would quote/escape it.
  context.print_line(context.current_path().string());
  return true;
}

} // namespace vfs9_cli
