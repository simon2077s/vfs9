/**
 * @file memfs.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Memory file system implementation
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "vfs9/detail/node_tree.hpp"
#include "vfs9/detail/type_traits.hpp"
#include "vfs9/file.hpp"
#include "vfs9/fs.hpp"
#include "vfs9/fs/mem/node.hpp"

namespace vfs9 {

/**
 * @brief Memory file system
 *
 */
struct memfs {};

namespace fs {
namespace mem {

/**
 * @brief Memory file system implementation
 *
 */
class fs_impl {
public:
  inline std::string_view name() const noexcept { return "mem"; }

  std::expected<void, std::error_code> init(memfs params) noexcept;

  bool exists(const std::filesystem::path& path) const noexcept;

  bool exists(const std::filesystem::path& path, node_type nt) const noexcept;

  bool exists(const std::filesystem::path& path, std::string_view name) const noexcept;

  bool exists(const std::filesystem::path& path, std::string_view name, node_type nt) const noexcept;

  bool exists(const std::filesystem::path& path,
              const std::filesystem::path::const_iterator& begin,
              const std::filesystem::path::const_iterator& end) const noexcept;

  bool exists(const std::filesystem::path& path,
              const std::filesystem::path::const_iterator& begin,
              const std::filesystem::path::const_iterator& end,
              node_type nt) const noexcept;

  node_status status(const std::filesystem::path& path) const noexcept;

  node_info get_info(const std::filesystem::path& path) const noexcept;

  std::expected<void, std::error_code> list(const std::filesystem::path& path,
                                            std::vector<node_entry>& out) const noexcept;

  std::expected<void, std::error_code> create_directory(const std::filesystem::path& path) noexcept;

  std::expected<void, std::error_code> create_directory(const std::filesystem::path& path, bool recursive) noexcept;

  std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                   placeholder_file_create_spec spec) noexcept;

  std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                   regular_file_create_spec spec) noexcept;

  std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                   scalar_file_create_spec spec) noexcept;

  std::expected<void, std::error_code> remove(const std::filesystem::path& path) noexcept;

  std::expected<void, std::error_code> remove(const std::filesystem::path& path, bool recursive) noexcept;

  std::expected<std::shared_ptr<stream_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                         stream_file_open_options options) noexcept;

  std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
  open_file(const std::filesystem::path& path, seq_stream_file_open_options options) noexcept;

  std::expected<std::shared_ptr<mapped_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                         mapped_file_open_options options) noexcept;

  std::expected<std::shared_ptr<scalar_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                         scalar_file_open_options options) noexcept;

  std::expected<std::shared_ptr<control_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                          control_file_open_options options) noexcept;

private:
  using tree_type = detail::node_tree<mem::node_base>;
  using node_ptr = tree_type::node_type;

  static node_ptr make_node(vfs9::node_type type, std::string name);

  std::expected<void, std::error_code>
  insert_node(const std::filesystem::path& path, node_ptr node, bool recursive) noexcept;

  std::expected<void, std::error_code> remove_node(const std::filesystem::path& path, bool recursive) noexcept;

  tree_type tree_; // Node tree
};

} // namespace mem
} // namespace fs

namespace detail {

template <>
struct fs_impl<memfs> {
  using type = ::vfs9::fs::mem::fs_impl;
};

} // namespace detail

static_assert(detail::is_fs<memfs>);

} // namespace vfs9
