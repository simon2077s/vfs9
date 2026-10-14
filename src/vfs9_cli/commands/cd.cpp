/**
 * @file cd.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `cd` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/cd.hpp"

#include <string>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool cd_command::run(cli& context, const std::vector<std::string>& args) {
  std::string path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("path", po::value<std::string>(&path), "target directory");

  po::positional_options_description positional;
  positional.add("path", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(args).options(desc).positional(positional).run(), vm);
    po::notify(vm);
  } catch (const po::error& e) {
    context.print_error(e.what());
    return true;
  }

  // Without an argument `cd` has no effect.
  if (!vm.count("path")) {
    return true;
  }

  const auto resolved = context.resolve_path(path);
  auto status = context.ns().status(resolved);
  if (!status) {
    context.print_error(status.error());
    return true;
  }
  if (status.value() == vfs9::node_status::not_found) {
    context.print_error(resolved.string() + ": no such directory");
    return true;
  }
  if (status.value() != vfs9::node_status::directory) {
    context.print_error(resolved.string() + " is not a directory");
    return true;
  }

  context.set_current_path(resolved);
  return true;
}

} // namespace vfs9_cli
