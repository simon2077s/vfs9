/**
 * @file main.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief VFS9 command line application
 * @version 0.1
 * @date 2026-10-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <exception>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "vfs9_cli/cli.hpp"

namespace po = boost::program_options;

int main(int argc, char** argv) {
  po::options_description desc("vfs9-cli options");
  // clang-format off
  desc.add_options()
      ("tracer,t", "enable the tracer (interface only, not implemented yet)")
      ("config,c", po::value<std::string>(), "load a config file (interface only, not implemented yet)")
      ("help,h", "show this help message");
  // clang-format on

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    std::cerr << "usage: vfs9-cli [options]\n\n" << desc << '\n';
    return 1;
  }

  if (vm.count("help") > 0) {
    std::cout << "usage: vfs9-cli [options]\n\n" << desc << '\n';
    return 0;
  }

  if (vm.count("tracer") > 0) {
    std::cerr << "warning: --tracer is not implemented yet\n";
  }
  if (vm.count("config") > 0) {
    std::cerr << "warning: --config is not implemented yet\n";
  }

  vfs9_cli::cli context;
  return context.run();
}
