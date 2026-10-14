/**
 * @file mkdir.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `mkdir` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/mkdir.hpp"

#include <filesystem>
#include <string>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool mkdir_command::run(cli& context, const std::vector<std::string>& args) {
  std::string path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("path", po::value<std::string>(&path), "directory path")(
      "parents,p", "create missing parent directories");

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

  if (!vm.count("path")) {
    context.print_error("mkdir requires a path");
    return true;
  }

  auto result = context.ns().create_directory(context.resolve_path(path), vm.count("parents") > 0);
  if (!result) {
    context.print_error(result.error());
  }
  return true;
}

} // namespace vfs9_cli
