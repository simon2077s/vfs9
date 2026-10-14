/**
 * @file buffer.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Buffer
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <span>

namespace vfs9 {

/**
 * @brief Mutable buffer
 *
 */
using mutable_buffer = std::span<std::byte>;

/**
 * @brief Const buffer
 *
 */
using const_buffer = std::span<const std::byte>;

/**
 * @brief Mutable buffer sequence
 *
 */
using mutable_buffer_sequence = std::span<mutable_buffer>;

/**
 * @brief Const buffer sequence
 *
 */
using const_buffer_sequence = std::span<const_buffer>;

} // namespace vfs9
