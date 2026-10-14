/**
 * @file ls.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `ls` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/ls.hpp"

#include <filesystem>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool ls_command::run(cli& context, const std::vector<std::string>& args) {
  std::string path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("path", po::value<std::string>(&path), "directory path");

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

  // No path means the current directory.
  auto entries = context.ns().list(context.resolve_path(path));
  if (!entries) {
    context.print_error(entries.error());
    return true;
  }

  for (const auto& entry : entries.value()) {
    if (entry.type == vfs9::node_type::directory) {
      std::cout << "directory  " << entry.name << '\n';
    } else {
      std::cout << "file       " << entry.name << " (" << to_string(entry.ft) << ")\n";
    }
  }
  return true;
}

} // namespace vfs9_cli
