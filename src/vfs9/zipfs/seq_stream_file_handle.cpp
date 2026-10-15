/**
 * @file seq_stream_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Sequential stream file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/zipfs/seq_stream_file_handle.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "vfs9/error.hpp"
#include "vfs9/zipfs/mz_util.hpp"

namespace vfs9 {
namespace zipfs_impl {

seq_stream_file_handle::seq_stream_file_handle(std::filesystem::path zip_path,
                                               std::filesystem::path path,
                                               std::string entry_name,
                                               size_t size)
    : base_file_handle(std::move(path), size), entry_name_(std::move(entry_name)) {
  p_reader_ = mz_zip_reader_create();
  if (p_reader_ == nullptr) {
    error_ = make_error_code(std::errc::not_enough_memory);
    return;
  }
  if (auto ec = mz_to_error(mz_zip_reader_open_file(p_reader_, zip_path.c_str())); ec) {
    error_ = ec;
  } else if (auto ec = mz_to_error(mz_zip_reader_locate_entry(p_reader_, entry_name_.c_str(), 0)); ec) {
    error_ = ec;
  } else if (auto ec = mz_to_error(mz_zip_reader_entry_open(p_reader_)); ec) {
    error_ = ec;
  }

  if (error_) {
    mz_zip_reader_close(p_reader_);
    mz_zip_reader_delete(&p_reader_);
  }
}

seq_stream_file_handle::~seq_stream_file_handle() {
  if (p_reader_ != nullptr) {
    mz_zip_reader_entry_close(p_reader_);
    mz_zip_reader_close(p_reader_);
    mz_zip_reader_delete(&p_reader_);
  }
}

size_t seq_stream_file_handle::max_size() const noexcept { return size_; }

size_t seq_stream_file_handle::tell_read() const noexcept { return read_cursor_; }

size_t seq_stream_file_handle::tell_write() const noexcept { return write_cursor_; }

bool seq_stream_file_handle::seekable() const noexcept { return true; }

std::error_code seq_stream_file_handle::reopen_entry() noexcept {
  // Best effort: closing a partially read entry reports MZ_CRC_ERROR because the checksum can only be
  // verified after the whole entry has been read. The close still resets the entry state.
  (void)mz_zip_reader_entry_close(p_reader_);
  if (auto ec = mz_to_error(mz_zip_reader_locate_entry(p_reader_, entry_name_.c_str(), 0)); ec) {
    return ec;
  }
  if (auto ec = mz_to_error(mz_zip_reader_entry_open(p_reader_)); ec) {
    return ec;
  }
  read_cursor_ = 0;
  return {};
}

std::error_code seq_stream_file_handle::skip_forward(size_t count) noexcept {
  std::array<std::byte, 16384> scratch{};
  while (count > 0) {
    const size_t chunk = std::min(count, scratch.size());
    const int32_t read = mz_zip_reader_entry_read(p_reader_, scratch.data(), static_cast<int32_t>(chunk));
    if (read < 0) {
      return mz_to_error(read);
    }
    if (read == 0) {
      return make_error_code(std::errc::io_error); // Unexpected end of entry
    }
    count -= static_cast<size_t>(read);
    read_cursor_ += static_cast<size_t>(read);
  }
  return {};
}

std::expected<void, std::error_code> seq_stream_file_handle::seek_read(int64_t offset,
                                                                       seq_stream_file::seek_whence whence) noexcept {
  if (error_) {
    return std::unexpected(error_);
  }
  if (p_reader_ == nullptr) {
    return std::unexpected(make_error_code(std::errc::io_error));
  }

  int64_t base = 0;
  switch (whence) {
  case seq_stream_file::seek_whence::begin:
    base = 0;
    break;
  case seq_stream_file::seek_whence::current:
    base = static_cast<int64_t>(read_cursor_);
    break;
  case seq_stream_file::seek_whence::end:
    base = static_cast<int64_t>(size_);
    break;
  }

  const int64_t target = base + offset;
  if (target < 0 || static_cast<size_t>(target) > size_) {
    return std::unexpected(make_error_code(errc::unsupported));
  }

  const size_t position = static_cast<size_t>(target);
  if (position == read_cursor_) {
    return {};
  }
  if (position > read_cursor_) {
    if (auto ec = skip_forward(position - read_cursor_); ec) {
      return std::unexpected(ec);
    }
    return {};
  }

  // Seeking backward re-opens the entry and skips forward from the beginning.
  if (auto ec = reopen_entry(); ec) {
    return std::unexpected(ec);
  }
  if (auto ec = skip_forward(position); ec) {
    return std::unexpected(ec);
  }
  return {};
}

std::expected<void, std::error_code> seq_stream_file_handle::seek_write(int64_t offset,
                                                                        seq_stream_file::seek_whence whence) noexcept {
  (void)offset;
  (void)whence;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<size_t, std::error_code> seq_stream_file_handle::read(mutable_buffer_sequence buffers) const noexcept {
  if (error_) {
    return std::unexpected(error_);
  }
  if (p_reader_ == nullptr) {
    return std::unexpected(make_error_code(std::errc::io_error));
  }

  size_t total = 0;
  for (auto& buffer : buffers) {
    size_t offset = 0;
    while (offset < buffer.size()) {
      const auto remaining = static_cast<int32_t>(std::min<size_t>(buffer.size() - offset, INT32_MAX));
      const int32_t read = mz_zip_reader_entry_read(p_reader_, buffer.data() + offset, remaining);
      if (read < 0) {
        return std::unexpected(mz_to_error(read));
      }
      if (read == 0) {
        read_cursor_ += total;
        return total;
      }
      offset += static_cast<size_t>(read);
      total += static_cast<size_t>(read);
    }
  }
  read_cursor_ += total;
  return total;
}

std::expected<size_t, std::error_code>
seq_stream_file_handle::write(const_buffer_sequence buffers) const noexcept {
  (void)buffers;
  return std::unexpected(make_error_code(errc::unsupported));
}

} // namespace zipfs_impl
} // namespace vfs9
