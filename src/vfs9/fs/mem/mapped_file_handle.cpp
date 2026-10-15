/**
 * @file mapped_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mapped file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/mem/mapped_file_handle.hpp"

namespace vfs9 {
namespace fs {
namespace mem {

size_t mapped_file_handle::max_size() const noexcept { return node_->size(); }

std::span<std::byte> mapped_file_handle::data() noexcept {
  auto* p_map = node_->map();
  if (p_map == nullptr) {
    return {};
  }
  return { reinterpret_cast<std::byte*>(p_map->address()), node_->size() };
}

std::span<const std::byte> mapped_file_handle::data() const noexcept {
  auto* p_map = node_->map();
  if (p_map == nullptr) {
    return {};
  }
  return { reinterpret_cast<const std::byte*>(p_map->address()), node_->size() };
}

size_t mapped_file_handle::reservation() const noexcept {
  auto* p_map = node_->map();
  return p_map != nullptr ? p_map->length() : 0;
}

size_t mapped_file_handle::starting_offset() const noexcept { return 0; }

} // namespace mem
} // namespace fs
} // namespace vfs9
