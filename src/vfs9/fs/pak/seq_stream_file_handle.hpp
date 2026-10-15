/**
 * @file seq_stream_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Sequential stream file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <system_error>

#include "vfs9/buffer.hpp"
#include "vfs9/detail/type_traits.hpp"
#include "vfs9/file.hpp"
#include "vfs9/fs/pak/stream_file_handle.hpp"

namespace vfs9 {
namespace fs {
namespace pak {

/**
 * @brief Sequential stream file handle
 *
 *  The payload is uncompressed, so seeking is fully supported. Writing is unsupported.
 *
 */
class seq_stream_file_handle : public stream_file_handle {
public:
  using stream_file_handle::stream_file_handle;

  /**
   * @brief Get read offset
   *
   * @return size_t
   */
  size_t tell_read() const noexcept;

  /**
   * @brief Get write offset
   *
   * @return size_t
   */
  size_t tell_write() const noexcept;

  /**
   * @brief Whether the file is seekable or not
   *
   * @return bool
   */
  bool seekable() const noexcept;

  /**
   * @brief Set read offset
   *
   * @param offset Offset
   * @param whence Seek whence
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> seek_read(int64_t offset, seq_stream_file::seek_whence whence) noexcept;

  /**
   * @brief Set write offset, always unsupported
   *
   * @param offset Offset
   * @param whence Seek whence
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> seek_write(int64_t offset, seq_stream_file::seek_whence whence) noexcept;

  /**
   * @brief Read from stream
   *
   * @param buffers Read to buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> read(mutable_buffer_sequence buffers) const noexcept;

  /**
   * @brief Write to stream, always unsupported
   *
   * @param buffers Write from buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> write(const_buffer_sequence buffers) const noexcept;

private:
  mutable size_t read_cursor_ = 0;  // Read cursor
  mutable size_t write_cursor_ = 0; // Write cursor
};

static_assert(detail::is_fs_seq_stream_file_handle<seq_stream_file_handle>);

} // namespace pak
} // namespace fs
} // namespace vfs9
