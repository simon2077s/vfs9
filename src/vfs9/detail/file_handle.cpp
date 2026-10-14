/**
 * @file file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File handle implementation
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/detail/file_handle.hpp"

namespace vfs9 {
namespace detail {

base_file_handle::~base_file_handle() = default;

stream_file_handle::~stream_file_handle() = default;

seq_stream_file_handle::~seq_stream_file_handle() = default;

mapped_file_handle::~mapped_file_handle() = default;

scalar_file_handle::~scalar_file_handle() = default;

control_file_handle::~control_file_handle() = default;

} // namespace detail
} // namespace vfs9
