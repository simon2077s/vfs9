/**
 * @file mount.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief `mount` command
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/commands/mount.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include <boost/program_options.hpp>

#include "vfs9/fs/local/fs.hpp"
#include "vfs9/fs/mem/fs.hpp"
#include "vfs9/fs/pak/fs.hpp"
#include "vfs9/fs/zip/fs.hpp"
#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

namespace vfs9_cli {

bool mount_command::run(cli& context, const std::vector<std::string>& args) {
  std::string fs_spec;
  std::string old_path;
  po::options_description desc{ std::string(usage()) };
  desc.add_options()("fs", po::value<std::string>(&fs_spec), "fs_name:name[:args]")(
      "old_path", po::value<std::string>(&old_path), "mount point")("after,a", "append to the union directory")(
      "before,b", "prepend to the union directory")("writable,w", "make the mounted entry writable");

  po::positional_options_description positional;
  positional.add("fs", 1).add("old_path", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(args).options(desc).positional(positional).run(), vm);
    po::notify(vm);
  } catch (const po::error& e) {
    context.print_error(e.what());
    return true;
  }

  if (!vm.count("fs") || !vm.count("old_path")) {
    context.print_error("mount requires fs_name:name[:args] and old_path");
    return true;
  }

  // fs_name:name[:args]
  auto parts = split(fs_spec, ':');
  if (parts.size() < 2 || parts[0].empty() || parts[1].empty()) {
    context.print_error("invalid file system spec, expected fs_name:name[:args]");
    return true;
  }
  const std::string& fs_name = parts[0];
  const std::string& name = parts[1];
  // Drop empty components so that `fs_name:name:` (or `fs_name:name`) means "no argument".
  std::vector<std::string> fs_args;
  for (auto it = parts.begin() + 2; it != parts.end(); ++it) {
    if (!it->empty()) {
      fs_args.push_back(*it);
    }
  }

  auto mflag = resolve_merge_flag(vm.count("before") > 0, vm.count("after") > 0);
  if (!mflag) {
    context.print_error(mflag.error());
    return true;
  }
  // -w only makes sense for union directories; a plain (replace) mount is writable by default.
  const bool writable = *mflag == vfs9::merge_flag::replace ? true : vm.count("writable") > 0;

  const auto mount_point = context.resolve_path(old_path);
  std::expected<void, std::error_code> result;
  if (fs_name == "mem") {
    if (!fs_args.empty()) {
      context.print_error("mem takes no argument");
      return true;
    }
    result = context.ns().mount(mount_point, *mflag, writable, name, vfs9::memfs{});
  } else if (fs_name == "local") {
    if (fs_args.size() != 1) {
      context.print_error("local requires exactly one argument: the root path");
      return true;
    }
    result = context.ns().mount(mount_point, *mflag, writable, name, vfs9::localfs{ fs_args[0] });
  } else if (fs_name == "zip") {
    if (fs_args.size() != 1) {
      context.print_error("zip requires exactly one argument: the archive path");
      return true;
    }
    result = context.ns().mount(mount_point, *mflag, writable, name, vfs9::zipfs{ fs_args[0] });
  } else if (fs_name == "pak") {
    if (fs_args.size() != 1) {
      context.print_error("pak requires exactly one argument: the archive path");
      return true;
    }
    result = context.ns().mount(mount_point, *mflag, writable, name, vfs9::pakfs{ fs_args[0] });
  } else {
    context.print_error("unknown file system: " + fs_name);
    return true;
  }

  if (!result) {
    context.print_error(result.error());
  }
  return true;
}

} // namespace vfs9_cli
