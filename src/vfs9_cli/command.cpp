/**
 * @file command.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief CLI command helpers and command registry
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/command.hpp"

#include <utility>

#include "vfs9_cli/commands/bind.hpp"
#include "vfs9_cli/commands/cd.hpp"
#include "vfs9_cli/commands/exit.hpp"
#include "vfs9_cli/commands/help.hpp"
#include "vfs9_cli/commands/ls.hpp"
#include "vfs9_cli/commands/mkdir.hpp"
#include "vfs9_cli/commands/mount.hpp"
#include "vfs9_cli/commands/pwd.hpp"
#include "vfs9_cli/commands/unmount.hpp"
#include "vfs9_cli/commands/unmount_all.hpp"

namespace vfs9_cli {

std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> parts;
  std::string current;
  for (const char ch : value) {
    if (ch == delimiter) {
      parts.push_back(std::move(current));
      current.clear();
    } else {
      current.push_back(ch);
    }
  }
  parts.push_back(std::move(current));
  return parts;
}

std::expected<vfs9::merge_flag, std::string> resolve_merge_flag(bool before, bool after) {
  if (before && after) {
    return std::unexpected("-b (before) and -a (after) can't be used at the same time");
  }
  if (before) {
    return vfs9::merge_flag::before;
  }
  if (after) {
    return vfs9::merge_flag::after;
  }
  return vfs9::merge_flag::replace;
}

std::string_view to_string(vfs9::file_type ft) noexcept {
  switch (ft) {
  case vfs9::file_type::placeholder:
    return "placeholder";
  case vfs9::file_type::regular:
    return "regular";
  case vfs9::file_type::scalar:
    return "scalar";
  case vfs9::file_type::control:
    return "control";
  }
  return "unknown";
}

namespace {

template <typename T>
void add_command(std::map<std::string, std::unique_ptr<command>>& commands) {
  auto cmd = std::make_unique<T>();
  const std::string name(cmd->name());
  commands.emplace(name, std::move(cmd));
}

} // namespace

std::map<std::string, std::unique_ptr<command>> make_commands() {
  std::map<std::string, std::unique_ptr<command>> commands;
  add_command<ls_command>(commands);
  add_command<cd_command>(commands);
  add_command<pwd_command>(commands);
  add_command<mkdir_command>(commands);
  add_command<mount_command>(commands);
  add_command<bind_command>(commands);
  add_command<unmount_command>(commands);
  add_command<unmount_all_command>(commands);
  add_command<help_command>(commands);
  add_command<exit_command>(commands);
  return commands;
}

} // namespace vfs9_cli
