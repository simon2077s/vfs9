/**
 * @file fs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File system
 * @version 0.1
 * @date 2026-09-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vfs9/detail/fs.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief File system global id
 *
 */
std::atomic<uint32_t> file_system::next_id_ = 0;

} // namespace detail
} // namespace vfs9
