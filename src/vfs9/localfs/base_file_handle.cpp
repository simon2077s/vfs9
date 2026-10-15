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
#include <string>
#include <system_error>
#include <typeinfo>
#include <utility>

namespace vfs9 {
namespace localfs_impl {

void open_registry::acquire(const std::filesystem::path& path) {
  std::lock_guard lock(mutex_);
  ++counts_[path];
}

void open_registry::release(const std::filesystem::path& path) {
  std::lock_guard lock(mutex_);
  auto it = counts_.find(path);
  if (it != counts_.end() && --it->second == 0) {
    counts_.erase(it);
  }
}

bool open_registry::has_open_at_or_below(const std::filesystem::path& path) const {
  std::lock_guard lock(mutex_);
  if (counts_.empty()) {
    return false;
  }
  const std::string base = path.generic_string();
  for (const auto& [key, count] : counts_) {
    if (count == 0) {
      continue;
    }
    const std::string candidate = key.generic_string();
    if (candidate == base) {
      return true;
    }
    if (base == "/" || (candidate.size() > base.size() && candidate.compare(0, base.size(), base) == 0 &&
                        candidate[base.size()] == '/')) {
      return true;
    }
  }
  return false;
}

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
  info.fs.name = "localfs";
  info.fs.path = path_;
  return info;
}

} // namespace localfs_impl
} // namespace vfs9
