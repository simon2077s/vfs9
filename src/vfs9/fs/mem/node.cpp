/**
 * @file node.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Memory file system node
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/mem/node.hpp"

#include <algorithm>
#include <utility>

namespace vfs9 {
namespace fs {
namespace mem {

std::error_code node_base::ensure_capacity(size_t needed) noexcept {
  static constexpr size_t min_section_size = 4096;
  needed = std::max(needed, min_section_size);

  if (!section_) {
    auto section_result = llfio::section_handle::section(needed);
    if (!section_result) {
      return make_error_code(section_result.error());
    }
    section_.emplace(std::move(section_result).value());
  } else {
    auto length_result = section_->length();
    if (!length_result) {
      return make_error_code(length_result.error());
    }
    if (length_result.value() < needed) {
      auto truncate_result = section_->truncate(needed);
      if (!truncate_result) {
        return make_error_code(truncate_result.error());
      }
    }
  }

  auto map_result = llfio::map_handle::map(*section_, 0);
  if (!map_result) {
    return make_error_code(map_result.error());
  }
  map_.emplace(std::move(map_result).value());
  return {};
}

} // namespace mem
} // namespace fs
} // namespace vfs9
