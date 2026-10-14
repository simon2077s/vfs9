/**
 * @file command.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief CLI command base class, helpers and command registry
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <expected>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "vfs9/fs.hpp"

namespace vfs9_cli {

class cli;

/**
 * @brief Base class of all CLI commands
 *
 *  Every command lives in its own translation unit and is registered in `make_commands`.
 *
 */
class command {
public:
  virtual ~command() = default;

  /**
   * @brief Command name used to dispatch from the REPL
   *
   * @return std::string_view
   */
  virtual std::string_view name() const noexcept = 0;

  /**
   * @brief One line usage string
   *
   * @return std::string_view
   */
  virtual std::string_view usage() const noexcept = 0;

  /**
   * @brief One line description
   *
   * @return std::string_view
   */
  virtual std::string_view description() const noexcept = 0;

  /**
   * @brief Run the command
   *
   * @param context CLI context
   * @param args Tokenized arguments (the command name is not included)
   * @return bool false to request the REPL to exit
   */
  virtual bool run(cli& context, const std::vector<std::string>& args) = 0;
};

/**
 * @brief Split a string by a delimiter
 *
 *  An empty input yields a single empty part, matching the behaviour of a plain split.
 *
 * @param value Value to split
 * @param delimiter Delimiter character
 * @return std::vector<std::string>
 */
std::vector<std::string> split(const std::string& value, char delimiter);

/**
 * @brief Resolve a merge flag from the -b (before) / -a (after) options
 *
 * @param before Whether -b was given
 * @param after Whether -a was given
 * @return std::expected<vfs9::merge_flag, std::string> Error message if both flags are given
 */
std::expected<vfs9::merge_flag, std::string> resolve_merge_flag(bool before, bool after);

/**
 * @brief Convert a file type to a human readable string
 *
 * @param ft File type
 * @return std::string_view
 */
std::string_view to_string(vfs9::file_type ft) noexcept;

/**
 * @brief Build the default command set
 *
 * @return std::map<std::string, std::unique_ptr<command>> Ordered by command name
 */
std::map<std::string, std::unique_ptr<command>> make_commands();

} // namespace vfs9_cli
