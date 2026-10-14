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

#include <filesystem>

#include "vfs9/fs.hpp"

namespace vfs9 {
namespace localfs_impl {

/**
 * @brief Base file handle
 *
 */
class base_file_handle {
public:
  /**
   * @brief Construct a new base file handle object
   *
   * @param local_path Local OS path
   * @param path Path relative to the file system root
   */
  base_file_handle(std::filesystem::path local_path, std::filesystem::path path);

  base_file_handle(const base_file_handle&) = delete;
  base_file_handle(base_file_handle&&) = default;

  virtual ~base_file_handle() = default;

  base_file_handle& operator=(const base_file_handle&) = delete;
  base_file_handle& operator=(base_file_handle&&) = default;

  /**
   * @brief Get the info
   *
   * @return node_info
   */
  node_info get_info() const noexcept;

protected:
  std::filesystem::path local_path_; // Local OS path
  std::filesystem::path path_;       // Path relative to the file system root
};

} // namespace localfs_impl
} // namespace vfs9
