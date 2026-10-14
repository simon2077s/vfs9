/**
 * @file unmount_all.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `unmount-all` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/unmount_all.hpp"

#include <filesystem>
#include <string>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool unmount_all_command::run(cli& context, const std::vector<std::string>& args) {
  std::string old_path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("old_path", po::value<std::string>(&old_path), "path to unmount (optional)");

  po::positional_options_description positional;
  positional.add("old_path", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(args).options(desc).positional(positional).run(), vm);
    po::notify(vm);
  } catch (const po::error& e) {
    context.print_error(e.what());
    return true;
  }

  if (vm.count("old_path")) {
    auto result = context.ns().unmount(context.resolve_path(old_path));
    if (!result) {
      context.print_error(result.error());
    } else if (*result) {
      context.print_line("unmounted all entries of " + old_path);
    } else {
      context.print_line("nothing to unmount on " + old_path);
    }
    return true;
  }

  auto result = context.ns().unmount();
  if (!result) {
    context.print_error(result.error());
  } else {
    context.print_line("unmounted all entries");
  }
  return true;
}

} // namespace vfs9_cli
