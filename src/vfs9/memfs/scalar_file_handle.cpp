/**
 * @file scalar_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Scalar file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/memfs/scalar_file_handle.hpp"

#include <utility>

#include "vfs9/error.hpp"

namespace vfs9 {
namespace memfs_impl {

const std::type_info& scalar_file_handle::type() const noexcept { return node_->scalar().type(); }

std::expected<std::any, std::error_code> scalar_file_handle::read() const noexcept { return node_->scalar(); }

std::expected<void, std::error_code> scalar_file_handle::write(std::any value) noexcept {
  if (value.type() != node_->scalar().type()) {
    return std::unexpected(make_error_code(errc::scalar_value_type_mismatch));
  }
  node_->scalar() = std::move(value);
  return {};
}

} // namespace memfs_impl
} // namespace vfs9
