/**
 * @file mapped_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mapped file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <span>

#include "vfs9/detail/type_traits.hpp"
#include "vfs9/fs/mem/base_file_handle.hpp"

namespace vfs9 {
namespace fs {
namespace mem {

/**
 * @brief Mapped file handle
 *
 */
class mapped_file_handle : public base_file_handle {
public:
  using base_file_handle::base_file_handle;

  /**
   * @brief File max size
   *
   * @return size_t
   */
  size_t max_size() const noexcept;

  /**
   * @brief Get mapped data
   *
   * @return std::span<std::byte>
   */
  std::span<std::byte> data() noexcept;

  /**
   * @brief Get mapped data
   *
   * @return std::span<const std::byte>
   */
  std::span<const std::byte> data() const noexcept;

  /**
   * @brief Get address space reservation
   *
   * @return size_t
   */
  size_t reservation() const noexcept;

  /**
   * @brief Get offset into the backing file where the mapping begins
   *
   * @return size_t
   */
  size_t starting_offset() const noexcept;
};

static_assert(detail::is_fs_mapped_file_handle<mapped_file_handle>);

} // namespace mem
} // namespace fs
} // namespace vfs9
