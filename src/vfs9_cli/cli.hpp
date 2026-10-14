/**
 * @file cli.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief vfs9 command line interface context and REPL
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "vfs9/vns.hpp"
#include "vfs9_cli/command.hpp"

namespace vfs9_cli {

/**
 * @brief CLI context
 *
 *  Owns the root vnamespace and the command set, and drives the interactive REPL.
 *
 */
class cli {
public:
  cli();
  ~cli();

  cli(const cli&) = delete;
  cli(cli&&) = delete;
  cli& operator=(const cli&) = delete;
  cli& operator=(cli&&) = delete;

  /**
   * @brief Run the REPL until `exit` or EOF
   *
   * @return int Process exit code
   */
  int run();

  /**
   * @brief Get the root namespace
   *
   * @return vfs9::vnamespace&
   */
  inline vfs9::vnamespace& ns() noexcept { return *p_ns_; }

  /**
   * @brief Get the root namespace
   *
   * @return const vfs9::vnamespace&
   */
  inline const vfs9::vnamespace& ns() const noexcept { return *p_ns_; }

  /**
   * @brief Print a line to stdout
   *
   * @param message
   */
  void print_line(std::string_view message) const;

  /**
   * @brief Print an error message to stderr
   *
   * @param message
   */
  void print_error(std::string_view message) const;

  /**
   * @brief Print an error code to stderr
   *
   * @param ec
   */
  void print_error(const std::error_code& ec) const;

  /**
   * @brief Get the registered commands
   *
   * @return const std::map<std::string, std::unique_ptr<command>>&
   */
  inline const std::map<std::string, std::unique_ptr<command>>& commands() const noexcept { return commands_; }

  /**
   * @brief Get the current (absolute) path
   *
   * @return const std::filesystem::path&
   */
  inline const std::filesystem::path& current_path() const noexcept { return current_path_; }

  /**
   * @brief Set the current path, expected to be an absolute directory path
   *
   * @param path
   */
  inline void set_current_path(std::filesystem::path path) { current_path_ = std::move(path); }

  /**
   * @brief Resolve a user supplied path against the current path
   *
   *  Absolute paths are returned as-is, relative paths are joined with the current path. An empty
   *  path resolves to the current path. The result is lexically normalized but not guaranteed to
   *  exist.
   *
   * @param path Path as typed by the user
   * @return std::filesystem::path
   */
  std::filesystem::path resolve_path(const std::string& path) const;

  /**
   * @brief Split a command line into tokens, honoring double quotes
   *
   * @param line
   * @return std::vector<std::string>
   */
  static std::vector<std::string> tokenize(const std::string& line);

private:
  std::shared_ptr<vfs9::vnamespace> p_ns_;
  std::map<std::string, std::unique_ptr<command>> commands_;
  std::filesystem::path current_path_ = "/";
};

} // namespace vfs9_cli
