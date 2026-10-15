/**
 * @file memfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Memory file system implementation
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/mem/fs.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

#include "vfs9/detail/file_handle.hpp"
#include "vfs9/error.hpp"
#include "vfs9/fs/mem/mapped_file_handle.hpp"
#include "vfs9/fs/mem/scalar_file_handle.hpp"
#include "vfs9/fs/mem/seq_stream_file_handle.hpp"
#include "vfs9/fs/mem/stream_file_handle.hpp"

namespace vfs9 {
namespace fs {
namespace mem {

fs_impl::node_ptr fs_impl::make_node(vfs9::node_type type, std::string name) {
  return std::make_shared<detail::node_tree_node<mem::node_base>>(type, std::move(name));
}

std::expected<void, std::error_code> fs_impl::init(memfs params) noexcept {
  (void)params;
  tree_ = tree_type(make_node(node_type::directory, "/"));
  return {};
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
    const std::type_info* scalar_type = &typeid(void);
    if (node->ft() == file_type::scalar) {
      scalar_type = &node->scalar().type();
    }
    info.file = node_info::file_info{ node->ft(), node->size(), scalar_type };
  }
  info.fs.name = "mem";
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

std::expected<void, std::error_code>
fs_impl::insert_node(const std::filesystem::path& path, node_ptr node, bool recursive) noexcept {
  if (path.empty()) {
    return std::unexpected(make_error_code(errc::empty_path));
  }
  if (!path.is_absolute()) {
    return std::unexpected(make_error_code(errc::not_an_abs_path));
  }
  if (tree_.find(path)) {
    return std::unexpected(make_error_code(std::errc::file_exists));
  }

  auto begin = path.begin();
  auto end = path.end();

  if (tree_.find(path.parent_path())) {
    if (!tree_.set(begin, end, std::move(node), false)) {
      return std::unexpected(make_error_code(errc::invalid_path));
    }
    return {};
  }

  if (!recursive) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  auto new_dir = [](std::string_view dir_name) -> node_ptr {
    return make_node(node_type::directory, std::string(dir_name));
  };
  if (!tree_.recursive_set(begin, end, new_dir, std::move(node), false)) {
    return std::unexpected(make_error_code(errc::invalid_path));
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::remove_node(const std::filesystem::path& path, bool recursive) noexcept {
  if (path.empty()) {
    return std::unexpected(make_error_code(errc::empty_path));
  }
  if (!path.is_absolute()) {
    return std::unexpected(make_error_code(errc::not_an_abs_path));
  }
  auto node = tree_.find(path);
  if (!node) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }

  // Refuse to remove a file which is currently open (or a directory containing such a file).
  auto has_open_file = [](auto&& self, const node_ptr& p_node) -> bool {
    if (p_node->type() == node_type::file) {
      return p_node->open_count() > 0;
    }
    bool found = false;
    p_node->children().for_each([&](std::string_view, const node_ptr& child) {
      if (!found && self(self, child)) {
        found = true;
      }
    });
    return found;
  };
  if (has_open_file(has_open_file, node)) {
    return std::unexpected(make_error_code(errc::file_in_use));
  }

  auto begin = path.begin();
  auto end = path.end();
  if (!tree_.erase(begin, end, recursive)) {
    return std::unexpected(make_error_code(std::errc::directory_not_empty));
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::create_directory(const std::filesystem::path& path) noexcept {
  auto name = path.filename().string();
  return insert_node(path, make_node(node_type::directory, std::move(name)), false);
}

std::expected<void, std::error_code> fs_impl::create_directory(const std::filesystem::path& path,
                                                               bool recursive) noexcept {
  auto name = path.filename().string();
  return insert_node(path, make_node(node_type::directory, std::move(name)), recursive);
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          placeholder_file_create_spec spec) noexcept {
  (void)spec;
  auto name = path.filename().string();
  auto node = make_node(node_type::file, std::move(name));
  node->mark_placeholder();
  return insert_node(path, std::move(node), false);
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          regular_file_create_spec spec) noexcept {
  auto name = path.filename().string();
  auto node = make_node(node_type::file, std::move(name));
  node->mark_regular(spec.initial_size);
  const size_t capacity = std::max(spec.initial_size, spec.reserve_size.value_or(0));
  if (auto ec = node->ensure_capacity(capacity)) {
    return std::unexpected(ec);
  }
  return insert_node(path, std::move(node), false);
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          scalar_file_create_spec spec) noexcept {
  auto name = path.filename().string();
  auto node = make_node(node_type::file, std::move(name));
  node->mark_scalar(std::move(spec.init_value));
  return insert_node(path, std::move(node), false);
}

std::expected<void, std::error_code> fs_impl::remove(const std::filesystem::path& path) noexcept {
  return remove_node(path, false);
}

std::expected<void, std::error_code> fs_impl::remove(const std::filesystem::path& path, bool recursive) noexcept {
  return remove_node(path, recursive);
}

std::expected<std::shared_ptr<stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, stream_file_open_options options) noexcept {
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  if (node->ft() != file_type::regular) {
    return std::unexpected(make_error_code(errc::unsupported));
  }
  auto handle = detail::make_stream_file_handle<mem::stream_file_handle>(options.mode, node, path);
  return std::make_shared<stream_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, seq_stream_file_open_options options) noexcept {
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  if (node->ft() != file_type::regular) {
    return std::unexpected(make_error_code(errc::unsupported));
  }
  auto handle = detail::make_seq_stream_file_handle<mem::seq_stream_file_handle>(options.mode, node, path);
  return std::make_shared<seq_stream_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<mapped_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, mapped_file_open_options options) noexcept {
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  if (node->ft() != file_type::regular) {
    return std::unexpected(make_error_code(errc::unsupported));
  }
  auto handle = detail::make_mapped_file_handle<mem::mapped_file_handle>(options.mode, node, path);
  return std::make_shared<mapped_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<scalar_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, scalar_file_open_options options) noexcept {
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  if (node->ft() != file_type::scalar) {
    return std::unexpected(make_error_code(errc::unsupported));
  }
  auto handle = detail::make_scalar_file_handle<mem::scalar_file_handle>(options.mode, node, path);
  return std::make_shared<scalar_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<control_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, control_file_open_options options) noexcept {
  (void)path;
  (void)options;
  return std::unexpected(make_error_code(errc::unsupported));
}

} // namespace mem
} // namespace fs
} // namespace vfs9
