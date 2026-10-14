/**
 * @file base_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Base file handle
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/localfs/base_file_handle.hpp"

#include <cstddef>
#include <system_error>
#include <typeinfo>
#include <utility>

namespace vfs9 {
namespace localfs_impl {

base_file_handle::base_file_handle(std::filesystem::path local_path, std::filesystem::path path)
    : local_path_(std::move(local_path)), path_(std::move(path)) {}

node_info base_file_handle::get_info() const noexcept {
  node_info info{};
  info.status = node_status::file;
  info.name = path_.filename().empty() ? std::string("/") : path_.filename().string();
  std::error_code ec;
  const auto size = std::filesystem::file_size(local_path_, ec);
  info.file = node_info::file_info{ file_type::regular, ec ? size_t{ 0 } : static_cast<size_t>(size), &typeid(void) };
  info.fs.name = "localfs";
  info.fs.path = path_;
  return info;
}

} // namespace localfs_impl
} // namespace vfs9
