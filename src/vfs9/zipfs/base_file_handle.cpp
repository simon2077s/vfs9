/**
 * @file base_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Base file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/zipfs/base_file_handle.hpp"

#include <cstddef>
#include <typeinfo>
#include <utility>

namespace vfs9 {
namespace zipfs_impl {

base_file_handle::base_file_handle(std::filesystem::path path, size_t size)
    : path_(std::move(path)), size_(size) {}

node_info base_file_handle::get_info() const noexcept {
  node_info info{};
  info.status = node_status::file;
  info.name = path_.filename().empty() ? std::string("/") : path_.filename().string();
  info.file = node_info::file_info{ file_type::regular, size_, &typeid(void) };
  info.fs.name = "zipfs";
  info.fs.path = path_;
  return info;
}

} // namespace zipfs_impl
} // namespace vfs9
