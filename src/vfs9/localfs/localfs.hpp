/**
 * @file localfs.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Local file system
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

#include "vfs9/detail/type_traits.hpp"
#include "vfs9/file.hpp"
#include "vfs9/fs.hpp"
#include "vfs9/localfs/base_file_handle.hpp"

namespace vfs9 {

/**
 * @brief Local file system
 *
 */
struct localfs {
  std::filesystem::path root; // The local root path of local file system, must be an absolute path.
};

namespace localfs_impl {

/**
 * @brief Local file system implementation
 *
 */
class fs_impl {
public:
  /**
   * @brief Get name
   *
   * @return std::string_view
   */
  inline std::string_view name() const noexcept { return "localfs"; }

  /**
   * @brief Initialize the local file system
   *
   * @param params File system parameters
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> init(localfs params) noexcept;

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
  /**
   * @brief Map a path relative to the file system root to a local path
   *
   * @param path Path relative to the file system root
   * @return std::filesystem::path
   */
  std::filesystem::path local_path(const std::filesystem::path& path) const noexcept;

  std::filesystem::path root_;                                               // Local root path
  std::shared_ptr<open_registry> open_registry_ = std::make_shared<open_registry>(); // Open file registry
};

} // namespace localfs_impl

namespace detail {

template <>
struct fs_impl<localfs> {
  using type = ::vfs9::localfs_impl::fs_impl;
};

} // namespace detail

static_assert(detail::is_fs<localfs>);

} // namespace vfs9
