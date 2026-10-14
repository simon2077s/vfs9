/**
 * @file scalar_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Scalar file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <any>
#include <expected>
#include <system_error>
#include <typeinfo>

#include "vfs9/detail/type_traits.hpp"
#include "vfs9/memfs/base_file_handle.hpp"

namespace vfs9 {
namespace memfs_impl {

/**
 * @brief Scalar file handle
 *
 */
class scalar_file_handle : public base_file_handle {
public:
  using base_file_handle::base_file_handle;

  /**
   * @brief Get scalar value type
   *
   * @return const std::type_info&
   */
  const std::type_info& type() const noexcept;

  /**
   * @brief Read scalar value
   *
   * @return std::expected<std::any, std::error_code>
   */
  std::expected<std::any, std::error_code> read() const noexcept;

  /**
   * @brief Write scalar value
   *
   * @param value Value to write
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> write(std::any value) noexcept;
};

static_assert(detail::is_fs_scalar_file_handle<scalar_file_handle>);

} // namespace memfs_impl
} // namespace vfs9
