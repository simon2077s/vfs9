/**
 * @file fs.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Pak file system
 * @version 0.1
 * @date 2026-10-15
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
#include "vfs9/fs/pak/node.hpp"

namespace vfs9 {

/**
 * @brief Pak file system
 *
 *  A read-only file system backed by a Quake / id Tech "PACK" archive. Entries are stored
 *  uncompressed, so files can be opened both as random access stream files and as sequential
 *  stream files. Placeholder / scalar / control files and mapped files are not supported, and no
 *  mutation is possible.
 *
 */
struct pakfs {
  std::filesystem::path path; // Path of the backing pak archive, must be an absolute path
};

namespace fs {
namespace pak {

/**
 * @brief Pak file system implementation
 *
 */
class fs_impl {
public:
  inline std::string_view name() const noexcept { return "pak"; }

  std::expected<void, std::error_code> init(pakfs params) noexcept;

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
  using tree_type = detail::node_tree<pak::node_base>;
  using node_ptr = tree_type::node_type;

  /**
   * @brief Create a new (empty) node
   *
   * @param type Node type
   * @param name Node name
   * @return node_ptr
   */
  static node_ptr make_node(vfs9::node_type type, std::string name);

  /**
   * @brief Insert a pak directory entry into the node tree
   *
   * @param filename Entry name inside the archive
   * @param size File size
   * @param offset Offset of the file data inside the archive
   * @param is_dir Whether the entry is a directory
   */
  void insert_entry(std::string_view filename, size_t size, size_t offset, bool is_dir);

  std::filesystem::path pak_path_; // Backing pak archive path
  tree_type tree_;                 // Node tree
};

} // namespace pak
} // namespace fs

namespace detail {

template <>
struct fs_impl<pakfs> {
  using type = ::vfs9::fs::pak::fs_impl;
};

} // namespace detail

static_assert(detail::is_fs<pakfs>);

} // namespace vfs9
