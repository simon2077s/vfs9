/**
 * @file stream_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Stream file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <expected>
#include <system_error>

#include "vfs9/buffer.hpp"
#include "vfs9/detail/type_traits.hpp"
#include "vfs9/fs/pak/base_file_handle.hpp"

namespace vfs9 {
namespace fs {
namespace pak {

/**
 * @brief Stream file handle
 *
 *  Random access reads are translated into reads of the entry region inside the pak archive.
 *
 */
class stream_file_handle : public base_file_handle {
public:
  using base_file_handle::base_file_handle;

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
   * @brief Write to file by an absolute offset, always unsupported
   *
   * @param offset Write to offset
   * @param buffers Write from buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> write(size_t offset, const_buffer_sequence buffers) const noexcept;
};

static_assert(detail::is_fs_stream_file_handle<stream_file_handle>);

} // namespace pak
} // namespace fs
} // namespace vfs9
