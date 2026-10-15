/**
 * @file base_file_handle.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Base file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/memfs/base_file_handle.hpp"

#include <typeinfo>
#include <utility>

namespace vfs9 {
namespace memfs_impl {

base_file_handle::base_file_handle(node_pointer node, std::filesystem::path path)
    : node_(std::move(node)), path_(std::move(path)) {
  if (node_) {
    node_->acquire();
  }
}

base_file_handle::~base_file_handle() {
  if (node_) {
    node_->release();
  }
}

node_info base_file_handle::get_info() const noexcept {
  node_info info{};
  info.status = node_->type() == node_type::directory ? node_status::directory : node_status::file;
  info.name = node_->name();
  if (info.status == node_status::file) {
    const std::type_info* scalar_type = &typeid(void);
    if (node_->ft() == file_type::scalar) {
      scalar_type = &node_->scalar().type();
    }
    info.file = node_info::file_info{ node_->ft(), node_->size(), scalar_type };
  }
  info.fs.name = "memfs";
  info.fs.path = path_;
  return info;
}

} // namespace memfs_impl
} // namespace vfs9
