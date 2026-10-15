/**
 * @file node.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Zip file system node
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>

#include "vfs9/fs.hpp"

namespace vfs9 {
namespace fs {
namespace zip {

/**
 * @brief Zip file system node base
 *
 *  A zip entry is always a read-only regular file (or a directory). Only the file type and the
 *  uncompressed size are stored; the payload itself lives in the backing zip archive.
 *
 */
class node_base {
public:
  node_base() = default;

  node_base(const node_base&) = delete;
  node_base(node_base&&) = default;

  ~node_base() = default;

  node_base& operator=(const node_base&) = delete;
  node_base& operator=(node_base&&) = default;

  /**
   * @brief Get file type
   *
   * @return file_type
   */
  inline file_type ft() const noexcept { return ft_; }

  /**
   * @brief Get uncompressed file size
   *
   * @return size_t
   */
  inline size_t size() const noexcept { return size_; }

  /**
   * @brief Mark this node as a regular file
   *
   * @param size Uncompressed size in bytes
   */
  inline void mark_regular(size_t size) noexcept {
    ft_ = file_type::regular;
    size_ = size;
  }

private:
  file_type ft_ = file_type::placeholder; // File type
  size_t size_ = 0;                       // Uncompressed file size
};

} // namespace zip
} // namespace fs
} // namespace vfs9
