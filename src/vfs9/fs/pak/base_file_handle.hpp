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

#include <llfio/llfio.hpp>

#include "vfs9/fs.hpp"

namespace llfio = LLFIO_V2_NAMESPACE;

namespace vfs9 {
namespace fs {
namespace pak {

/**
 * @brief Base file handle
 *
 *  Owns an llfio handle on the backing pak archive which is used for random access reads.
 *
 */
class base_file_handle {
public:
  /**
   * @brief Construct a new base file handle object
   *
   * @param pak_path Path of the backing pak archive
   * @param path Path relative to the file system root
   * @param offset Offset of the file data inside the pak archive
   * @param size File size
   */
  base_file_handle(std::filesystem::path pak_path, std::filesystem::path path, size_t offset, size_t size);

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
  mutable llfio::file_handle file_; // Backing pak archive handle
  std::filesystem::path path_;      // Path relative to the file system root
  size_t offset_;                   // Offset of the file data inside the pak archive
  size_t size_;                     // File size
};

} // namespace pak
} // namespace fs
} // namespace vfs9
