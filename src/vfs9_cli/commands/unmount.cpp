/**
 * @file unmount.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `unmount` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/unmount.hpp"

#include <filesystem>
#include <string>
#include <string_view>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool unmount_command::run(cli& context, const std::vector<std::string>& args) {
  std::string target;
  std::string old_path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("target", po::value<std::string>(&target), "mount name or bind source path")(
      "old_path", po::value<std::string>(&old_path), "path the entry was mounted/bound to");

  po::positional_options_description positional;
  positional.add("target", 1).add("old_path", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(args).options(desc).positional(positional).run(), vm);
    po::notify(vm);
  } catch (const po::error& e) {
    context.print_error(e.what());
    return true;
  }

  if (!vm.count("target") || !vm.count("old_path")) {
    context.print_error("unmount requires name/new_path and old_path");
    return true;
  }

  const auto old_path_fs = context.resolve_path(old_path);

  // Try to remove by mount/bind name first (bind stores the source path as its name).
  auto by_name = context.ns().unmount(std::string_view(target), old_path_fs);
  if (!by_name) {
    context.print_error(by_name.error());
    return true;
  }
  if (*by_name) {
    context.print_line("unmounted");
    return true;
  }

  // Fall back to removing by source path (absolute or relative to the current path).
  auto by_path = context.ns().unmount(context.resolve_path(target), old_path_fs);
  if (!by_path) {
    context.print_error(by_path.error());
    return true;
  }
  if (*by_path) {
    context.print_line("unmounted");
    return true;
  }

  context.print_error("no mount or bind entry named '" + target + "' on " + old_path_fs.string());
  return true;
}

} // namespace vfs9_cli
