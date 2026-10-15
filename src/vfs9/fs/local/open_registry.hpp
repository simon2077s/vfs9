/**
 * @file open_registry.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Open registry
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <filesystem>
#include <map>
#include <mutex>

namespace vfs9 {
namespace fs {
namespace local {

/**
 * @brief Registry of currently open files, keyed by file system relative path
 *
 *  localfs has no in-memory nodes, so open files are tracked here to be able to refuse removing a
 *  file which is still open. Shared between the file system implementation and its file handles.
 *
 */
class open_registry {
public:
  open_registry() = default;

  open_registry(const open_registry&) = delete;
  open_registry(open_registry&&) = delete;
  open_registry& operator=(const open_registry&) = delete;
  open_registry& operator=(open_registry&&) = delete;

  ~open_registry() = default;

  /**
   * @brief Register one more open handle for a path
   *
   * @param path Path relative to the file system root
   */
  void acquire(const std::filesystem::path& path);

  /**
   * @brief Release one open handle for a path
   *
   * @param path Path relative to the file system root
   */
  void release(const std::filesystem::path& path);

  /**
   * @brief Whether a path, or any path below it, has an open handle
   *
   * @param path Path relative to the file system root
   * @return bool
   */
  bool has_open_at_or_below(const std::filesystem::path& path) const;

private:
  mutable std::mutex mutex_;
  std::map<std::filesystem::path, size_t> counts_;
};

} // namespace local
} // namespace fs
} // namespace vfs9
