/**
 * @file bind.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `bind` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/bind.hpp"

#include <filesystem>
#include <string>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool bind_command::run(cli& context, const std::vector<std::string>& args) {
  std::string new_path;
  std::string old_path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("new_path", po::value<std::string>(&new_path), "source path")(
      "old_path", po::value<std::string>(&old_path), "destination path")("after,a", "append to the union directory")(
      "before,b", "prepend to the union directory")("writable,w", "make the bound entry writable");

  po::positional_options_description positional;
  positional.add("new_path", 1).add("old_path", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(args).options(desc).positional(positional).run(), vm);
    po::notify(vm);
  } catch (const po::error& e) {
    context.print_error(e.what());
    return true;
  }

  if (!vm.count("new_path") || !vm.count("old_path")) {
    context.print_error("bind requires new_path and old_path");
    return true;
  }

  auto mflag = resolve_merge_flag(vm.count("before") > 0, vm.count("after") > 0);
  if (!mflag) {
    context.print_error(mflag.error());
    return true;
  }
  // -w only makes sense for union directories; a plain (replace) bind is writable by default.
  const bool writable = *mflag == vfs9::merge_flag::replace ? true : vm.count("writable") > 0;

  auto result = context.ns().bind(
      context.resolve_path(new_path), context.resolve_path(old_path), *mflag, writable);
  if (!result) {
    context.print_error(result.error());
  }
  return true;
}

} // namespace vfs9_cli
