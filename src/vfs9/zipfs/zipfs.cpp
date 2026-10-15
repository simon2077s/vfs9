/**
 * @file zipfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief zip file system
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/zipfs/zipfs.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "vfs9/detail/file_handle.hpp"
#include "vfs9/error.hpp"
#include "vfs9/zipfs/mz_util.hpp"
#include "vfs9/zipfs/seq_stream_file_handle.hpp"

namespace vfs9 {
namespace zipfs_impl {

fs_impl::node_ptr fs_impl::make_node(vfs9::node_type type, std::string name) {
  return std::make_shared<detail::node_tree_node<zipfs_impl::node_base>>(type, std::move(name));
}

std::expected<void, std::error_code> fs_impl::init(zipfs params) noexcept {
  if (params.path.empty() || !params.path.is_absolute()) {
    return std::unexpected(make_error_code(errc::not_an_abs_path));
  }

  std::error_code ec;
  if (!std::filesystem::exists(params.path, ec) || ec) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  if (!std::filesystem::is_regular_file(params.path, ec) || ec) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  zip_path_ = std::move(params.path);

  tree_ = tree_type(make_node(node_type::directory, "/"));

  void* p_reader = mz_zip_reader_create();
  if (p_reader == nullptr) {
    return std::unexpected(make_error_code(std::errc::not_enough_memory));
  }

  int32_t mzec = mz_zip_reader_open_file(p_reader, zip_path_.c_str());
  if (mzec != MZ_OK) {
    mz_zip_reader_delete(&p_reader);
    return std::unexpected(mz_to_error(mzec));
  }

  mzec = mz_zip_reader_goto_first_entry(p_reader);
  if (mzec == MZ_OK) {
    do {
      mz_zip_file* info = nullptr;
      if (mz_zip_reader_entry_get_info(p_reader, &info) != MZ_OK || info == nullptr) {
        break;
      }
      const auto size = info->uncompressed_size < 0 ? size_t{ 0 } : static_cast<size_t>(info->uncompressed_size);
      const bool is_dir = mz_zip_reader_entry_is_dir(p_reader) == MZ_OK;
      insert_entry(info->filename != nullptr ? info->filename : "", is_dir, size);
    } while (mz_zip_reader_goto_next_entry(p_reader) == MZ_OK);
  } else if (mzec != MZ_END_OF_LIST) {
    mz_zip_reader_close(p_reader);
    mz_zip_reader_delete(&p_reader);
    return std::unexpected(mz_to_error(mzec));
  }

  mz_zip_reader_close(p_reader);
  mz_zip_reader_delete(&p_reader);
  return {};
}

void fs_impl::insert_entry(std::string_view filename, bool is_dir, size_t size) {
  std::string name(filename);
  while (!name.empty() && name.front() == '/') {
    name.erase(name.begin());
  }
  while (!name.empty() && (name.back() == '/' || name.back() == '\\')) {
    name.pop_back();
  }
  if (name.empty()) {
    return;
  }

  // NOTE: `entry_path` owns the iterators passed to `recursive_set`, keep it alive for the call.
  std::filesystem::path entry_path(name);
  auto begin = entry_path.begin();
  auto end = entry_path.end();

  auto new_dir = [](std::string_view dir_name) -> node_ptr {
    return make_node(node_type::directory, std::string(dir_name));
  };

  auto node = make_node(is_dir ? node_type::directory : node_type::file, entry_path.filename().string());
  if (!is_dir) {
    node->mark_regular(size);
  }
  tree_.recursive_set(begin, end, new_dir, std::move(node), false);
}

bool fs_impl::exists(const std::filesystem::path& path) const noexcept { return tree_.find(path) != nullptr; }

bool fs_impl::exists(const std::filesystem::path& path, node_type nt) const noexcept {
  auto node = tree_.find(path);
  return node != nullptr && node->type() == nt;
}

bool fs_impl::exists(const std::filesystem::path& path, std::string_view name) const noexcept {
  auto node = tree_.find(path);
  return node != nullptr && node->children().contains(name);
}

bool fs_impl::exists(const std::filesystem::path& path, std::string_view name, node_type nt) const noexcept {
  auto node = tree_.find(path);
  if (!node) {
    return false;
  }
  auto child = node->children().find(name);
  return child != nullptr && child->type() == nt;
}

bool fs_impl::exists(const std::filesystem::path& path,
                     const std::filesystem::path::const_iterator& begin,
                     const std::filesystem::path::const_iterator& end) const noexcept {
  return tree_type::find(tree_.find(path), begin, end) != nullptr;
}

bool fs_impl::exists(const std::filesystem::path& path,
                     const std::filesystem::path::const_iterator& begin,
                     const std::filesystem::path::const_iterator& end,
                     node_type nt) const noexcept {
  auto node = tree_type::find(tree_.find(path), begin, end);
  return node != nullptr && node->type() == nt;
}

node_status fs_impl::status(const std::filesystem::path& path) const noexcept {
  auto node = tree_.find(path);
  if (!node) {
    return node_status::not_found;
  }
  return node->type() == node_type::directory ? node_status::directory : node_status::file;
}

node_info fs_impl::get_info(const std::filesystem::path& path) const noexcept {
  node_info info{};
  auto node = tree_.find(path);
  if (!node) {
    info.status = node_status::not_found;
    return info;
  }
  info.status = node->type() == node_type::directory ? node_status::directory : node_status::file;
  info.name = node->name();
  if (info.status == node_status::file) {
    info.file = node_info::file_info{ node->ft(), node->size(), &typeid(void) };
  }
  info.fs.name = "zipfs";
  info.fs.path = path;
  return info;
}

std::expected<void, std::error_code> fs_impl::list(const std::filesystem::path& path,
                                                   std::vector<node_entry>& out) const noexcept {
  auto node = tree_.find(path);
  if (!node) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  if (node->type() != node_type::directory) {
    return std::unexpected(make_error_code(errc::not_a_dir));
  }

  out.clear();
  node->children().for_each([&out](std::string_view name, const node_ptr& child) {
    node_entry entry{};
    entry.type = child->type() == node_type::directory ? node_type::directory : node_type::file;
    entry.name = std::string(name);
    if (entry.type == node_type::file) {
      entry.ft = child->ft();
    }
    out.push_back(std::move(entry));
  });
  return {};
}

std::expected<void, std::error_code> fs_impl::create_directory(const std::filesystem::path& path) noexcept {
  (void)path;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::create_directory(const std::filesystem::path& path,
                                                               bool recursive) noexcept {
  (void)path;
  (void)recursive;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          placeholder_file_create_spec spec) noexcept {
  (void)path;
  (void)spec;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          regular_file_create_spec spec) noexcept {
  (void)path;
  (void)spec;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          scalar_file_create_spec spec) noexcept {
  (void)path;
  (void)spec;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::remove(const std::filesystem::path& path) noexcept {
  (void)path;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::remove(const std::filesystem::path& path, bool recursive) noexcept {
  (void)path;
  (void)recursive;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<std::shared_ptr<stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, stream_file_open_options options) noexcept {
  (void)path;
  (void)options;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, seq_stream_file_open_options options) noexcept {
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  std::string entry_name = path.relative_path().generic_string();
  auto handle = detail::make_seq_stream_file_handle<zipfs_impl::seq_stream_file_handle>(
      options.mode, zip_path_, path, std::move(entry_name), node->size());
  return std::make_shared<seq_stream_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<mapped_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, mapped_file_open_options options) noexcept {
  (void)path;
  (void)options;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<std::shared_ptr<scalar_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, scalar_file_open_options options) noexcept {
  (void)path;
  (void)options;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<std::shared_ptr<control_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, control_file_open_options options) noexcept {
  (void)path;
  (void)options;
  return std::unexpected(make_error_code(errc::unsupported));
}

} // namespace zipfs_impl
} // namespace vfs9
