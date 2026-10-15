/**
 * @file stream_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Stream file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/pak/stream_file_handle.hpp"

#include <algorithm>
#include <vector>

#include "vfs9/error.hpp"

namespace vfs9 {
namespace fs {
namespace pak {

size_t stream_file_handle::max_size() const noexcept { return size_; }

std::expected<size_t, std::error_code> stream_file_handle::read(size_t offset,
                                                                mutable_buffer_sequence buffers) const noexcept {
  if (offset >= size_) {
    return size_t{ 0 };
  }
  if (!file_.is_valid()) {
    return std::unexpected(make_error_code(std::errc::io_error));
  }

  // Clamp the requested buffers to the entry bounds so a read never crosses into another entry.
  size_t remaining = size_ - offset;
  std::vector<llfio::byte_io_handle::buffer_type> llfio_buffers;
  llfio_buffers.reserve(buffers.size());
  for (auto buffer : buffers) {
    if (remaining == 0) {
      break;
    }
    const size_t n = std::min(buffer.size(), remaining);
    llfio_buffers.emplace_back(buffer.data(), n);
    remaining -= n;
  }
  if (llfio_buffers.empty()) {
    return size_t{ 0 };
  }

  llfio::byte_io_handle::io_request<llfio::byte_io_handle::buffers_type> request(
      llfio::byte_io_handle::buffers_type(llfio_buffers), offset_ + offset);
  auto result = file_.read(request);
  if (!result) {
    return std::unexpected(make_error_code(result.error()));
  }
  return result.bytes_transferred();
}

std::expected<size_t, std::error_code>
stream_file_handle::write(size_t offset, const_buffer_sequence buffers) const noexcept {
  (void)offset;
  (void)buffers;
  return std::unexpected(make_error_code(errc::unsupported));
}

} // namespace pak
} // namespace fs
} // namespace vfs9
