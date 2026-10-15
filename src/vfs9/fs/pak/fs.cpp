/**
 * @file fs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Pak file system
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/pak/fs.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "vfs9/detail/file_handle.hpp"
#include "vfs9/error.hpp"
#include "vfs9/fs/pak/seq_stream_file_handle.hpp"
#include "vfs9/fs/pak/stream_file_handle.hpp"

namespace vfs9 {
namespace fs {
namespace pak {

namespace {

// Quake pak directory entry: char name[56]; int filepos; int filelen (little endian).
constexpr size_t kPakHeaderSize = 12;
constexpr size_t kPakNameSize = 56;
constexpr size_t kPakEntrySize = 64;
constexpr char kPakMagic[4] = { 'P', 'A', 'C', 'K' };

/**
 * @brief Read a little endian 32-bit unsigned integer
 *
 * @param data Pointer to at least 4 bytes
 * @return std::uint32_t
 */
std::uint32_t read_u32_le(const unsigned char* data) noexcept {
  return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
         (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
}

} // namespace

fs_impl::node_ptr fs_impl::make_node(vfs9::node_type type, std::string name) {
  return std::make_shared<detail::node_tree_node<pak::node_base>>(type, std::move(name));
}

std::expected<void, std::error_code> fs_impl::init(pakfs params) noexcept {
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
  pak_path_ = std::move(params.path);

  std::ifstream in(pak_path_, std::ios::binary);
  if (!in) {
    return std::unexpected(make_error_code(std::errc::io_error));
  }

  std::array<unsigned char, kPakHeaderSize> header{};
  in.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
  if (in.gcount() != static_cast<std::streamsize>(header.size()) ||
      std::memcmp(header.data(), kPakMagic, sizeof(kPakMagic)) != 0) {
    return std::unexpected(make_error_code(errc::unsupported));
  }

  const std::uint32_t dir_offset = read_u32_le(header.data() + 4);
  const std::uint32_t dir_length = read_u32_le(header.data() + 8);
  if (dir_length % kPakEntrySize != 0) {
    return std::unexpected(make_error_code(errc::unsupported));
  }

  const auto file_size = std::filesystem::file_size(pak_path_, ec);
  if (ec) {
    return std::unexpected(make_error_code(std::errc::io_error));
  }
  if (static_cast<std::uint64_t>(dir_offset) + dir_length > file_size) {
    return std::unexpected(make_error_code(errc::unsupported));
  }

  std::vector<unsigned char> directory(dir_length);
  in.seekg(dir_offset, std::ios::beg);
  in.read(reinterpret_cast<char*>(directory.data()), static_cast<std::streamsize>(dir_length));
  if (in.gcount() != static_cast<std::streamsize>(dir_length)) {
    return std::unexpected(make_error_code(std::errc::io_error));
  }

  tree_ = tree_type(make_node(node_type::directory, "/"));

  const size_t count = dir_length / kPakEntrySize;
  for (size_t i = 0; i < count; ++i) {
    const unsigned char* entry = directory.data() + i * kPakEntrySize;

    size_t name_length = 0;
    while (name_length < kPakNameSize && entry[name_length] != 0) {
      ++name_length;
    }
    const std::string_view raw_name(reinterpret_cast<const char*>(entry), name_length);
    const std::uint32_t file_pos = read_u32_le(entry + kPakNameSize);
    const std::uint32_t file_len = read_u32_le(entry + kPakNameSize + 4);
    const bool is_dir = !raw_name.empty() && (raw_name.back() == '/' || raw_name.back() == '\\');

    insert_entry(raw_name, static_cast<size_t>(file_len), static_cast<size_t>(file_pos), is_dir);
  }
  return {};
}

void fs_impl::insert_entry(std::string_view filename, size_t size, size_t offset, bool is_dir) {
  std::string name(filename);
  for (auto& ch : name) {
    if (ch == '\\') {
      ch = '/';
    }
  }
  while (!name.empty() && name.front() == '/') {
    name.erase(name.begin());
  }
  while (!name.empty() && name.back() == '/') {
    name.pop_back();
    is_dir = true;
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
    node->mark_regular(size, offset);
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
  info.fs.name = "pak";
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
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  auto handle = detail::make_stream_file_handle<pak::stream_file_handle>(
      options.mode, pak_path_, path, node->offset(), node->size());
  return std::make_shared<stream_file>(options.mode, std::move(handle));
}

std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
fs_impl::open_file(const std::filesystem::path& path, seq_stream_file_open_options options) noexcept {
  auto node = tree_.find(path);
  if (!node || node->type() != node_type::file) {
    return std::unexpected(make_error_code(errc::not_a_file));
  }
  auto handle = detail::make_seq_stream_file_handle<pak::seq_stream_file_handle>(
      options.mode, pak_path_, path, node->offset(), node->size());
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

} // namespace pak
} // namespace fs
} // namespace vfs9
