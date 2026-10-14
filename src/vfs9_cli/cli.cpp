/**
 * @file cli.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief vfs9 command line interface context and REPL
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9_cli/cli.hpp"

#include <cctype>
#include <iostream>
#include <utility>

#include "linenoise.hpp"

namespace vfs9_cli {

std::filesystem::path cli::resolve_path(const std::string& path) const {
  if (path.empty()) {
    return current_path_;
  }
  std::filesystem::path p(path);
  if (p.is_absolute()) {
    return p.lexically_normal();
  }
  return (current_path_ / p).lexically_normal();
}

cli::cli() : p_ns_(vfs9::make_namespace()), commands_(make_commands()) {}

cli::~cli() = default;

void cli::print_line(std::string_view message) const { std::cout << message << '\n'; }

void cli::print_error(std::string_view message) const { std::cerr << "error: " << message << '\n'; }

void cli::print_error(const std::error_code& ec) const {
  std::cerr << "error: " << ec.message() << " (code " << ec.value() << ")\n";
}

std::vector<std::string> cli::tokenize(const std::string& line) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_quote = false;
  bool has_token = false;
  for (const char ch : line) {
    if (in_quote) {
      if (ch == '"') {
        in_quote = false;
      } else {
        current.push_back(ch);
      }
      continue;
    }
    if (ch == '"') {
      in_quote = true;
      has_token = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      if (has_token) {
        tokens.push_back(std::move(current));
        current.clear();
        has_token = false;
      }
      continue;
    }
    current.push_back(ch);
    has_token = true;
  }
  if (has_token) {
    tokens.push_back(std::move(current));
  }
  return tokens;
}

int cli::run() {
  // Single line commands; up/down arrows navigate the history that we feed below.
  linenoise::SetMultiLine(false);

  std::string line;
  while (true) {
    // Readline returns true when the user wants to quit (Ctrl-D, Ctrl-C, or EOF).
    if (linenoise::Readline("> ", line)) {
      std::cout << '\n';
      break;
    }
    if (line.empty()) {
      continue;
    }
    linenoise::AddHistory(line.c_str());

    auto tokens = tokenize(line);
    if (tokens.empty()) {
      continue;
    }

    const auto it = commands_.find(tokens[0]);
    if (it == commands_.end()) {
      print_error("unknown command: " + tokens[0]);
      continue;
    }

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    if (!it->second->run(*this, args)) {
      break;
    }
  }
  return 0;
}

} // namespace vfs9_cli
