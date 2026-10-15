/**
 * @file node.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Pak file system node
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
namespace pak {

/**
 * @brief Pak file system node base
 *
 *  A pak entry is always a read-only regular file (or a directory). The payload is stored
 *  uncompressed in the backing pak archive, so a node only needs to remember where it lives.
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
   * @brief Get file size
   *
   * @return size_t
   */
  inline size_t size() const noexcept { return size_; }

  /**
   * @brief Get the offset of the file data inside the backing pak archive
   *
   * @return size_t
   */
  inline size_t offset() const noexcept { return offset_; }

  /**
   * @brief Mark this node as a regular file
   *
   * @param size File size in bytes
   * @param offset Offset of the file data inside the pak archive
   */
  inline void mark_regular(size_t size, size_t offset) noexcept {
    ft_ = file_type::regular;
    size_ = size;
    offset_ = offset;
  }

private:
  file_type ft_ = file_type::placeholder; // File type
  size_t size_ = 0;                       // File size
  size_t offset_ = 0;                     // Offset inside the backing pak archive
};

} // namespace pak
} // namespace fs
} // namespace vfs9
