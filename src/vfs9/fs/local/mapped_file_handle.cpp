/**
 * @file mapped_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mapped file handle
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/local/mapped_file_handle.hpp"

#include <utility>

namespace vfs9 {
namespace fs {
namespace local {

mapped_file_handle::mapped_file_handle(std::shared_ptr<open_registry> registry,
                                       llfio::mapped_file_handle mapped,
                                       std::filesystem::path local_path,
                                       std::filesystem::path path)
    : base_file_handle(std::move(registry), std::move(local_path), std::move(path)), mapped_(std::move(mapped)) {}

size_t mapped_file_handle::max_size() const noexcept {
  auto result = mapped_.maximum_extent();
  return result ? static_cast<size_t>(result.value()) : size_t{ 0 };
}

std::span<std::byte> mapped_file_handle::data() noexcept {
  auto* address = mapped_.address();
  if (address == nullptr) {
    return {};
  }
  return { reinterpret_cast<std::byte*>(address), max_size() };
}

std::span<const std::byte> mapped_file_handle::data() const noexcept {
  auto* address = mapped_.address();
  if (address == nullptr) {
    return {};
  }
  return { reinterpret_cast<const std::byte*>(address), max_size() };
}

size_t mapped_file_handle::reservation() const noexcept { return mapped_.capacity(); }

size_t mapped_file_handle::starting_offset() const noexcept { return static_cast<size_t>(mapped_.starting_offset()); }

} // namespace local
} // namespace fs
} // namespace vfs9
