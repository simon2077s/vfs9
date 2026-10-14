/**
 * @file stream_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Stream file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/memfs/stream_file_handle.hpp"

#include <algorithm>
#include <cstring>

namespace vfs9 {
namespace memfs_impl {

size_t stream_file_handle::max_size() const noexcept { return node_->size(); }

std::expected<size_t, std::error_code> stream_file_handle::read(size_t offset,
                                                                mutable_buffer_sequence buffers) const noexcept {
  if (offset >= node_->size()) {
    return size_t{ 0 };
  }
  auto* p_map = node_->map();
  if (p_map == nullptr) {
    return size_t{ 0 };
  }

  const auto* base = reinterpret_cast<const std::byte*>(p_map->address());
  const size_t available = node_->size() - offset;
  size_t copied = 0;
  for (auto& buffer : buffers) {
    const size_t n = std::min(buffer.size(), available - copied);
    if (n > 0) {
      std::memcpy(buffer.data(), base + offset + copied, n);
      copied += n;
    }
    if (copied >= available) {
      break;
    }
  }
  return copied;
}

std::expected<size_t, std::error_code> stream_file_handle::write(size_t offset,
                                                                 const_buffer_sequence buffers) const noexcept {
  size_t total = 0;
  for (const auto& buffer : buffers) {
    total += buffer.size();
  }
  if (total == 0) {
    return size_t{ 0 };
  }
  if (auto ec = node_->ensure_capacity(offset + total)) {
    return std::unexpected(ec);
  }

  auto* p_map = node_->map();
  auto* base = reinterpret_cast<std::byte*>(p_map->address());
  size_t written = 0;
  for (const auto& buffer : buffers) {
    if (buffer.size() > 0) {
      std::memcpy(base + offset + written, buffer.data(), buffer.size());
      written += buffer.size();
    }
  }
  if (offset + written > node_->size()) {
    node_->set_size(offset + written);
  }
  return written;
}

} // namespace memfs_impl
} // namespace vfs9
