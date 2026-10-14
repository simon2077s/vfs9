/**
 * @file file.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File implementation
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/file.hpp"

#include <utility>

#include "vfs9/detail/file_handle.hpp"

namespace vfs9 {

// ------ stream_file

size_t stream_file::max_size() const noexcept { return handle().max_size(); }

std::expected<size_t, std::error_code> stream_file::read(size_t offset,
                                                         mutable_buffer_sequence buffers) const noexcept {
  return handle().read(offset, buffers);
}

std::expected<size_t, std::error_code> stream_file::write(size_t offset, const_buffer_sequence buffers) const noexcept {
  return handle().write(offset, buffers);
}

// ------ seq_stream_file

size_t seq_stream_file::max_size() const noexcept { return handle().max_size(); }

size_t seq_stream_file::tell_read() const noexcept { return handle().tell_read(); }

size_t seq_stream_file::tell_write() const noexcept { return handle().tell_write(); }

bool seq_stream_file::seekable() const noexcept { return handle().seekable(); }

std::expected<void, std::error_code> seq_stream_file::seek_read(int64_t offset,
                                                                seq_stream_file::seek_whence whence) noexcept {
  return handle().seek_read(offset, whence);
}

std::expected<void, std::error_code> seq_stream_file::seek_write(int64_t offset,
                                                                 seq_stream_file::seek_whence whence) noexcept {
  return handle().seek_write(offset, whence);
}

std::expected<size_t, std::error_code> seq_stream_file::read(mutable_buffer_sequence buffers) noexcept {
  return handle().read(buffers);
}

std::expected<size_t, std::error_code> seq_stream_file::write(const_buffer_sequence buffers) noexcept {
  return handle().write(buffers);
}

// ------ mapped_file

size_t mapped_file::max_size() const noexcept { return handle().max_size(); }

std::span<std::byte> mapped_file::data() noexcept { return handle().data(); }

std::span<const std::byte> mapped_file::data() const noexcept { return handle().data(); }

size_t mapped_file::reservation() const noexcept { return handle().reservation(); }

size_t mapped_file::starting_offset() const noexcept { return handle().starting_offset(); }

// ------ scalar_file

const std::type_info& scalar_file::type() const noexcept { return handle().type(); }

std::expected<std::any, std::error_code> scalar_file::read() const noexcept { return handle().read(); }

std::expected<void, std::error_code> scalar_file::write(std::any value) noexcept {
  return handle().write(std::move(value));
}

// ------ control_file

std::expected<void, std::error_code> control_file::execute(std::string_view command) noexcept {
  return handle().execute(command);
}

} // namespace vfs9
