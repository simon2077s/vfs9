/**
 * @file seq_stream_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Sequential stream file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/memfs/seq_stream_file_handle.hpp"

#include "vfs9/error.hpp"

namespace vfs9 {
namespace memfs_impl {

size_t seq_stream_file_handle::tell_read() const noexcept { return read_cursor_; }

size_t seq_stream_file_handle::tell_write() const noexcept { return write_cursor_; }

bool seq_stream_file_handle::seekable() const noexcept { return true; }

std::expected<void, std::error_code> seq_stream_file_handle::seek_read(int64_t offset,
                                                                       seq_stream_file::seek_whence whence) noexcept {
  int64_t base = 0;
  switch (whence) {
  case seq_stream_file::seek_whence::begin:
    base = 0;
    break;
  case seq_stream_file::seek_whence::current:
    base = static_cast<int64_t>(read_cursor_);
    break;
  case seq_stream_file::seek_whence::end:
    base = static_cast<int64_t>(node_->size());
    break;
  }
  const int64_t position = base + offset;
  if (position < 0) {
    return std::unexpected(make_error_code(errc::invalid_path));
  }
  read_cursor_ = static_cast<size_t>(position);
  return {};
}

std::expected<void, std::error_code> seq_stream_file_handle::seek_write(int64_t offset,
                                                                        seq_stream_file::seek_whence whence) noexcept {
  int64_t base = 0;
  switch (whence) {
  case seq_stream_file::seek_whence::begin:
    base = 0;
    break;
  case seq_stream_file::seek_whence::current:
    base = static_cast<int64_t>(write_cursor_);
    break;
  case seq_stream_file::seek_whence::end:
    base = static_cast<int64_t>(node_->size());
    break;
  }
  const int64_t position = base + offset;
  if (position < 0) {
    return std::unexpected(make_error_code(errc::invalid_path));
  }
  write_cursor_ = static_cast<size_t>(position);
  return {};
}

std::expected<size_t, std::error_code> seq_stream_file_handle::read(mutable_buffer_sequence buffers) const noexcept {
  auto result = stream_file_handle::read(read_cursor_, buffers);
  if (result) {
    read_cursor_ += result.value();
  }
  return result;
}

std::expected<size_t, std::error_code> seq_stream_file_handle::write(const_buffer_sequence buffers) const noexcept {
  auto result = stream_file_handle::write(write_cursor_, buffers);
  if (result) {
    write_cursor_ += result.value();
  }
  return result;
}

} // namespace memfs_impl
} // namespace vfs9
