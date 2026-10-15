/**
 * @file localfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Local file system
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/local/fs.hpp"

#include <memory>
#include <string>
#include <utility>

#include <llfio/llfio.hpp>

#include "vfs9/detail/file_handle.hpp"
#include "vfs9/error.hpp"
#include "vfs9/fs/local/mapped_file_handle.hpp"
#include "vfs9/fs/local/seq_stream_file_handle.hpp"
#include "vfs9/fs/local/stream_file_handle.hpp"

namespace llfio = LLFIO_V2_NAMESPACE;

namespace vfs9 {
namespace fs {
namespace local {

namespace {

llfio::file_handle::mode to_mode(file_open_mode mode) noexcept {
  switch (mode) {
  case file_open_mode::read:
    return llfio::file_handle::mode::read;
  case file_open_mode::write:
    return llfio::file_handle::mode::write;
  case file_open_mode::append:
    return llfio::file_handle::mode::append;
  }
  return llfio::file_handle::mode::read;
}

llfio::file_handle::caching to_caching(file_caching caching) noexcept {
  switch (caching) {
  case file_caching::none:
    return llfio::file_handle::caching::none;
  case file_caching::only_metadata:
    return llfio::file_handle::caching::only_metadata;
  case file_caching::reads:
    return llfio::file_handle::caching::reads;
  case file_caching::reads_and_metadata:
    return llfio::file_handle::caching::reads_and_metadata;
  case file_caching::all:
    return llfio::file_handle::caching::all;
  case file_caching::safety_barriers:
    return llfio::file_handle::caching::safety_barriers;
  case file_caching::temporary:
    return llfio::file_handle::caching::temporary;
  }
  return llfio::file_handle::caching::all;
}

llfio::file_handle::flag to_flag(file_flag flags) noexcept {
  llfio::file_handle::flag result = llfio::file_handle::flag::none;
  if ((flags & file_flag::unlink_on_first_close) != file_flag::none) {
    result |= llfio::file_handle::flag::unlink_on_first_close;
  }
  if ((flags & file_flag::disable_safety_barriers) != file_flag::none) {
    result |= llfio::file_handle::flag::disable_safety_barriers;
  }
  if ((flags & file_flag::disable_safety_unlinks) != file_flag::none) {
    result |= llfio::file_handle::flag::disable_safety_unlinks;
  }
  if ((flags & file_flag::disable_prefetching) != file_flag::none) {
    result |= llfio::file_handle::flag::disable_prefetching;
  }
  if ((flags & file_flag::maximum_prefetching) != file_flag::none) {
    result |= llfio::file_handle::flag::maximum_prefetching;
  }
  if ((flags & file_flag::multiplexable) != file_flag::none) {
    result |= llfio::file_handle::flag::multiplexable;
  }
  return result;
}

} // namespace

std::filesystem::path fs_impl::local_path(const std::filesystem::path& path) const noexcept {
  return root_ / path.relative_path();
}

std::expected<void, std::error_code> fs_impl::init(localfs params) noexcept {
  if (params.root.empty() || !params.root.is_absolute()) {
    return std::unexpected(make_error_code(errc::not_an_abs_path));
  }
  std::error_code ec;
  if (!std::filesystem::exists(params.root, ec) || ec) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  if (!std::filesystem::is_directory(params.root, ec) || ec) {
    return std::unexpected(make_error_code(errc::not_a_dir));
  }
  root_ = std::move(params.root);
  return {};
}

node_status fs_impl::status(const std::filesystem::path& path) const noexcept {
  std::error_code ec;
  auto st = std::filesystem::status(local_path(path), ec);
  if (ec) {
    return node_status::not_found;
  }
  if (std::filesystem::is_directory(st)) {
    return node_status::directory;
  }
  if (std::filesystem::is_regular_file(st)) {
    return node_status::file;
  }
  return node_status::not_found;
}

bool fs_impl::exists(const std::filesystem::path& path) const noexcept {
  return status(path) != node_status::not_found;
}

bool fs_impl::exists(const std::filesystem::path& path, node_type nt) const noexcept {
  const auto st = status(path);
  if (st == node_status::directory) {
    return nt == node_type::directory;
  }
  if (st == node_status::file) {
    return nt == node_type::file;
  }
  return false;
}

bool fs_impl::exists(const std::filesystem::path& path, std::string_view name) const noexcept {
  return status(path / std::string(name)) != node_status::not_found;
}

bool fs_impl::exists(const std::filesystem::path& path, std::string_view name, node_type nt) const noexcept {
  return exists(path / std::string(name), nt);
}

bool fs_impl::exists(const std::filesystem::path& path,
                     const std::filesystem::path::const_iterator& begin,
                     const std::filesystem::path::const_iterator& end) const noexcept {
  std::filesystem::path combined = path;
  for (auto it = begin; it != end; ++it) {
    if (*it == "/") {
      continue;
    }
    combined /= *it;
  }
  return status(combined) != node_status::not_found;
}

bool fs_impl::exists(const std::filesystem::path& path,
                     const std::filesystem::path::const_iterator& begin,
                     const std::filesystem::path::const_iterator& end,
                     node_type nt) const noexcept {
  std::filesystem::path combined = path;
  for (auto it = begin; it != end; ++it) {
    if (*it == "/") {
      continue;
    }
    combined /= *it;
  }
  return exists(combined, nt);
}

node_info fs_impl::get_info(const std::filesystem::path& path) const noexcept {
  node_info info{};
  info.status = status(path);
  info.name = path.filename().empty() ? std::string("/") : path.filename().string();
  if (info.status == node_status::file) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(local_path(path), ec);
    info.file = node_info::file_info{ file_type::regular, ec ? size_t{ 0 } : static_cast<size_t>(size), &typeid(void) };
  }
  info.fs.name = "local";
  info.fs.path = path;
  return info;
}

std::expected<void, std::error_code> fs_impl::list(const std::filesystem::path& path,
                                                   std::vector<node_entry>& out) const noexcept {
  if (status(path) != node_status::directory) {
    return std::unexpected(make_error_code(errc::not_a_dir));
  }

  std::error_code ec;
  std::filesystem::directory_iterator it(local_path(path), ec);
  const std::filesystem::directory_iterator end;
  if (ec) {
    return std::unexpected(ec);
  }

  out.clear();
  for (; it != end; it.increment(ec)) {
    if (ec) {
      return std::unexpected(ec);
    }
    const auto& entry = *it;
    std::error_code st_ec;
    const auto st = entry.status(st_ec);
    if (st_ec) {
      continue;
    }
    node_entry ne{};
    if (std::filesystem::is_directory(st)) {
      ne.type = node_type::directory;
    } else if (std::filesystem::is_regular_file(st)) {
      ne.type = node_type::file;
      ne.ft = file_type::regular;
    } else {
      continue;
    }
    ne.name = entry.path().filename().string();
    out.push_back(std::move(ne));
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::create_directory(const std::filesystem::path& path) noexcept {
  std::error_code ec;
  if (!std::filesystem::create_directory(local_path(path), ec) && !ec) {
    ec = make_error_code(std::errc::file_exists);
  }
  if (ec) {
    return std::unexpected(ec);
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::create_directory(const std::filesystem::path& path,
                                                               bool recursive) noexcept {
  std::error_code ec;
  if (recursive) {
    if (!std::filesystem::create_directories(local_path(path), ec) && !ec) {
      ec = make_error_code(std::errc::file_exists);
    }
  } else {
    if (!std::filesystem::create_directory(local_path(path), ec) && !ec) {
      ec = make_error_code(std::errc::file_exists);
    }
  }
  if (ec) {
    return std::unexpected(ec);
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          placeholder_file_create_spec spec) noexcept {
  (void)spec;
  auto file = llfio::file_handle::file({},
                                       local_path(path),
                                       llfio::file_handle::mode::write,
                                       llfio::file_handle::creation::only_if_not_exist,
                                       llfio::file_handle::caching::all,
                                       llfio::file_handle::flag::none);
  if (!file) {
    return std::unexpected(make_error_code(file.error()));
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          regular_file_create_spec spec) noexcept {
  auto file = llfio::file_handle::file({},
                                       local_path(path),
                                       llfio::file_handle::mode::write,
                                       llfio::file_handle::creation::only_if_not_exist,
                                       llfio::file_handle::caching::all,
                                       llfio::file_handle::flag::none);
  if (!file) {
    return std::unexpected(make_error_code(file.error()));
  }
  if (spec.initial_size > 0) {
    auto truncate_result = file.value().truncate(spec.initial_size);
    if (!truncate_result) {
      return std::unexpected(make_error_code(truncate_result.error()));
    }
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::create_file(const std::filesystem::path& path,
                                                          scalar_file_create_spec spec) noexcept {
  (void)path;
  (void)spec;
  return std::unexpected(make_error_code(errc::unsupported));
}

std::expected<void, std::error_code> fs_impl::remove(const std::filesystem::path& path) noexcept {
  if (open_registry_->has_open_at_or_below(path)) {
    return std::unexpected(make_error_code(errc::file_in_use));
  }
  std::error_code ec;
  if (!std::filesystem::remove(local_path(path), ec)) {
    if (!ec) {
      ec = make_error_code(errc::path_not_found);
    }
    return std::unexpected(ec);
  }
  return {};
}

std::expected<void, std::error_code> fs_impl::remove(const std::filesystem::path& path, bool recursive) noexcept {
  if (open_registry_->has_open_at_or_below(path)) {
    return std::unexpected(make_error_code(errc::file_in_use));
  }
  std::error_code ec;
  if (recursive) {
    if (!std::filesystem::remove_all(local_path(path), ec)) {
      if (!ec) {
        ec = make_error_code(errc::path_not_found);
      }
      return std::unexpected(ec);
    }
  } else {
    if (!std::filesystem::remove(local_path(path), ec)) {
      if (!ec) {
        ec = make_error_code(errc::path_not_found);
      }
      return std::unexpected(ec);
    }
  }
  return {};
}

std::expected<std::shared_ptr<stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, stream_file_open_options options) noexcept {
  if (status(path) != node_status::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  const auto local = local_path(path);
  auto file = llfio::file_handle::file({},
                                       local,
                                       to_mode(options.mode),
                                       llfio::file_handle::creation::open_existing,
                                       to_caching(options.caching),
                                       to_flag(options.flags));
  if (!file) {
    return std::unexpected(make_error_code(file.error()));
  }
  auto handle = detail::make_stream_file_handle<local::stream_file_handle>(
      options.mode, open_registry_, std::move(file).value(), local, path);
  return std::make_shared<stream_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, seq_stream_file_open_options options) noexcept {
  if (status(path) != node_status::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  const auto local = local_path(path);
  auto file = llfio::file_handle::file({},
                                       local,
                                       to_mode(options.mode),
                                       llfio::file_handle::creation::open_existing,
                                       to_caching(options.caching),
                                       to_flag(options.flags));
  if (!file) {
    return std::unexpected(make_error_code(file.error()));
  }
  auto handle = detail::make_seq_stream_file_handle<local::seq_stream_file_handle>(
      options.mode, open_registry_, std::move(file).value(), local, path);
  return std::make_shared<seq_stream_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<mapped_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, mapped_file_open_options options) noexcept {
  if (status(path) != node_status::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  if (options.mode == file_open_mode::append) {
    return std::unexpected(make_error_code(errc::unsupported));
  }
  const auto local = local_path(path);
  auto mapped = llfio::mapped_file_handle::mapped_file(options.reservation,
                                                       {},
                                                       local,
                                                       to_mode(options.mode),
                                                       llfio::mapped_file_handle::creation::open_existing,
                                                       to_caching(options.caching),
                                                       to_flag(options.flags),
                                                       llfio::section_handle::flag::none,
                                                       options.offset);
  if (!mapped) {
    return std::unexpected(make_error_code(mapped.error()));
  }
  auto handle = detail::make_mapped_file_handle<local::mapped_file_handle>(
      options.mode, open_registry_, std::move(mapped).value(), local, path);
  return std::make_shared<mapped_file>(options.mode, std::move(handle));
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

} // namespace local
} // namespace fs
} // namespace vfs9
