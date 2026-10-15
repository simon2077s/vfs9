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
#include "vfs9/fs/local/base_file_handle.hpp"

#include <cstddef>
#include <string>
#include <system_error>
#include <utility>

namespace vfs9 {
namespace fs {
namespace local {

base_file_handle::base_file_handle(std::shared_ptr<open_registry> registry,
                                   std::filesystem::path local_path,
                                   std::filesystem::path path)
    : registry_(std::move(registry)), local_path_(std::move(local_path)), path_(std::move(path)) {
  if (registry_) {
    registry_->acquire(path_);
  }
}

base_file_handle::~base_file_handle() {
  if (registry_) {
    registry_->release(path_);
  }
}

node_info base_file_handle::get_info() const noexcept {
  node_info info{};
  info.status = node_status::file;
  info.name = path_.filename().empty() ? std::string("/") : path_.filename().string();
  std::error_code ec;
  const auto size = std::filesystem::file_size(local_path_, ec);
  info.file = node_info::file_info{ file_type::regular, ec ? size_t{ 0 } : static_cast<size_t>(size), &typeid(void) };
  info.fs.name = "local";
  info.fs.path = path_;
  return info;
}

} // namespace local
} // namespace fs
} // namespace vfs9
