/**
 * @file stream_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Stream file handle
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/local/stream_file_handle.hpp"

#include <utility>
#include <vector>

namespace vfs9 {
namespace fs {
namespace local {

stream_file_handle::stream_file_handle(std::shared_ptr<open_registry> registry,
                                       llfio::file_handle file,
                                       std::filesystem::path local_path,
                                       std::filesystem::path path)
    : base_file_handle(std::move(registry), std::move(local_path), std::move(path)), file_(std::move(file)) {}

size_t stream_file_handle::max_size() const noexcept {
  auto result = file_.maximum_extent();
  return result ? static_cast<size_t>(result.value()) : size_t{ 0 };
}

std::expected<size_t, std::error_code> stream_file_handle::read(size_t offset,
                                                                mutable_buffer_sequence buffers) const noexcept {
  std::vector<llfio::byte_io_handle::buffer_type> llfio_buffers;
  llfio_buffers.reserve(buffers.size());
  for (auto buffer : buffers) {
    llfio_buffers.emplace_back(buffer.data(), buffer.size());
  }
  llfio::byte_io_handle::io_request<llfio::byte_io_handle::buffers_type> request(
      llfio::byte_io_handle::buffers_type(llfio_buffers), offset);
  auto result = file_.read(request);
  if (!result) {
    return std::unexpected(make_error_code(result.error()));
  }
  return result.bytes_transferred();
}

std::expected<size_t, std::error_code> stream_file_handle::write(size_t offset,
                                                                 const_buffer_sequence buffers) const noexcept {
  std::vector<llfio::byte_io_handle::const_buffer_type> llfio_buffers;
  llfio_buffers.reserve(buffers.size());
  for (auto buffer : buffers) {
    llfio_buffers.emplace_back(buffer.data(), buffer.size());
  }
  llfio::byte_io_handle::io_request<llfio::byte_io_handle::const_buffers_type> request(
      llfio::byte_io_handle::const_buffers_type(llfio_buffers), offset);
  auto result = file_.write(request);
  if (!result) {
    return std::unexpected(make_error_code(result.error()));
  }
  return result.bytes_transferred();
}

} // namespace local
} // namespace fs
} // namespace vfs9
