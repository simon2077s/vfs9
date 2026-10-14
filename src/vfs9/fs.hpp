/**
 * @file fs.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File system
 * @version 0.1
 * @date 2026-09-28
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace vfs9 {

/**
 * @brief Node type
 *
 */
enum class node_type {
  file,
  directory,
};

/**
 * @brief Node status
 *
 */
enum class node_status {
  not_found,
  file,
  directory,
};

/**
 * @brief File type
 *
 */
enum class file_type {
  placeholder, // A placeholder file, can't read or write
  regular,     // A regular file which supports read & write binary data
  scalar,      // A scalar file which supports read & write by specific type
  control,     // A control file which receives input string as a command and outputs execution result
};

/**
 * @brief Node entry (which always be returned by list)
 *
 */
struct node_entry {
  node_type type;   // Node type, either directory or file
  std::string name; // Directory or file name
  file_type ft;     // If type == file
};

/**
 * @brief Node info, returned by get_info() api
 *
 */
struct node_info {
  node_status status; // Status
  std::string name;   // Directory or file name

  struct file_info {
    file_type type;                    // File type
    size_t size;                       // File size
    const std::type_info* scalar_type; // Scalar data type if type == scalar, &typeid(void) otherwise
  };

  std::optional<file_info> file; // File info if type == file

  struct fs_info {
    std::string_view name;      // Name of file system, for debug purpose
    std::filesystem::path path; // The path of node relative to file system root
  };

  fs_info fs;                                          // File system info
  std::unordered_map<std::string, std::string> extras; // Extra info data
};

/**
 * @brief Merge flag
 *
 *  The actual behaviour of binding / mounting operation is defined by merge_flag:
 *
 *    - replace, old directory / file will be replaced by new directory / file
 *    - before, old directory will become a union - directory and new path will be added before any other paths. NOTE:
 * new and old must be directory.
 *    - after, the same as before except new path will be added after any other paths.
 *
 */
enum class merge_flag {
  /**
   * @brief Replace old path by new path, both file & directory can use this flag
   *
   */
  replace,
  /**
   * @brief Add new path before any other paths, only directory can use this flag.
   *
   */
  before,
  /**
   * @brief Add new path after any other paths, only directory can use this flag.
   *
   */
  after,
};

} // namespace vfs9
