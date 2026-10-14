/**
 * @file stream_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Stream file handle
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <system_error>

#include <llfio/llfio.hpp>

#include "vfs9/buffer.hpp"
#include "vfs9/detail/type_traits.hpp"
#include "vfs9/localfs/base_file_handle.hpp"

namespace llfio = LLFIO_V2_NAMESPACE;

namespace vfs9 {
namespace localfs_impl {

/**
 * @brief Stream file handle
 *
 */
class stream_file_handle : public base_file_handle {
public:
  /**
   * @brief Construct a new stream file handle object
   *
   * @param file Local file handle
   * @param local_path Local OS path
   * @param path Path relative to the file system root
   */
  stream_file_handle(llfio::file_handle file, std::filesystem::path local_path, std::filesystem::path path);

  /**
   * @brief File max size
   *
   * @return size_t
   */
  size_t max_size() const noexcept;

  /**
   * @brief Read from file by an absolute offset
   *
   * @param offset Read from offset
   * @param buffers Read to buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> read(size_t offset, mutable_buffer_sequence buffers) const noexcept;

  /**
   * @brief Write to file by an absolute offset
   *
   * @param offset Write to offset
   * @param buffers Write from buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> write(size_t offset, const_buffer_sequence buffers) const noexcept;

private:
  mutable llfio::file_handle file_; // Local file handle
};

static_assert(detail::is_fs_stream_file_handle<stream_file_handle>);

} // namespace localfs_impl
} // namespace vfs9
