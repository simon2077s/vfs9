/**
 * @file base_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Base file handle
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>

#include "vfs9/fs.hpp"

namespace vfs9 {
namespace localfs_impl {

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

/**
 * @brief Base file handle
 *
 */
class base_file_handle {
public:
  /**
   * @brief Construct a new base file handle object
   *
   * @param registry Open file registry
   * @param local_path Local OS path
   * @param path Path relative to the file system root
   */
  base_file_handle(std::shared_ptr<open_registry> registry,
                   std::filesystem::path local_path,
                   std::filesystem::path path);

  base_file_handle(const base_file_handle&) = delete;
  base_file_handle(base_file_handle&&) = default;

  virtual ~base_file_handle();

  base_file_handle& operator=(const base_file_handle&) = delete;
  base_file_handle& operator=(base_file_handle&&) = default;

  /**
   * @brief Get the info
   *
   * @return node_info
   */
  node_info get_info() const noexcept;

protected:
  std::shared_ptr<open_registry> registry_; // Open file registry
  std::filesystem::path local_path_;        // Local OS path
  std::filesystem::path path_;              // Path relative to the file system root
};

} // namespace localfs_impl
} // namespace vfs9
