/**
 * @file base_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Base file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <filesystem>

#include "vfs9/fs.hpp"

namespace vfs9 {
namespace fs {
namespace zip {

/**
 * @brief Base file handle
 *
 */
class base_file_handle {
public:
  /**
   * @brief Construct a new base file handle object
   *
   * @param path Path relative to the file system root
   * @param size Uncompressed file size
   */
  base_file_handle(std::filesystem::path path, size_t size);

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
  std::filesystem::path path_; // Path relative to the file system root
  size_t size_;                // Uncompressed file size
};

} // namespace zip
} // namespace fs
} // namespace vfs9
