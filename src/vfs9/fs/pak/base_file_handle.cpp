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
#include "vfs9/fs/pak/base_file_handle.hpp"

#include <typeinfo>
#include <utility>

namespace vfs9 {
namespace fs {
namespace pak {

base_file_handle::base_file_handle(std::filesystem::path pak_path,
                                   std::filesystem::path path,
                                   size_t offset,
                                   size_t size)
    : path_(std::move(path)), offset_(offset), size_(size) {
  auto file = llfio::file_handle::file({},
                                       pak_path,
                                       llfio::file_handle::mode::read,
                                       llfio::file_handle::creation::open_existing,
                                       llfio::file_handle::caching::all,
                                       llfio::file_handle::flag::none);
  if (file) {
    file_ = std::move(file).value();
  }
}

node_info base_file_handle::get_info() const noexcept {
  node_info info{};
  info.status = node_status::file;
  info.name = path_.filename().empty() ? std::string("/") : path_.filename().string();
  info.file = node_info::file_info{ file_type::regular, size_, &typeid(void) };
  info.fs.name = "pak";
  info.fs.path = path_;
  return info;
}

} // namespace pak
} // namespace fs
} // namespace vfs9
