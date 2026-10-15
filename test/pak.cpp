/**
 * @file pak.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Pak file system unit test cases
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vfs9/fs/pak/fs.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "vfs9/error.hpp"
#include "vfs9/vns.hpp"

namespace {

using namespace vfs9;

struct pak_file {
  std::string name;
  std::string data;
};

void write_u32_le(std::ofstream& out, std::uint32_t value) {
  const char bytes[4] = { static_cast<char>(value & 0xff), static_cast<char>((value >> 8) & 0xff),
                          static_cast<char>((value >> 16) & 0xff), static_cast<char>((value >> 24) & 0xff) };
  out.write(bytes, 4);
}

void write_pak(const std::filesystem::path& path, const std::vector<pak_file>& files) {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> positions;
  std::uint32_t offset = 12;
  for (const auto& file : files) {
    positions.emplace_back(offset, static_cast<std::uint32_t>(file.data.size()));
    offset += static_cast<std::uint32_t>(file.data.size());
  }
  const std::uint32_t dir_offset = offset;
  const std::uint32_t dir_length = static_cast<std::uint32_t>(64 * files.size());

  std::ofstream out(path, std::ios::binary);
  out.write("PACK", 4);
  write_u32_le(out, dir_offset);
  write_u32_le(out, dir_length);
  for (const auto& file : files) {
    out.write(file.data.data(), static_cast<std::streamsize>(file.data.size()));
  }
  for (size_t i = 0; i < files.size(); ++i) {
    char name[56] = {};
    std::memcpy(name, files[i].name.data(), std::min<size_t>(files[i].name.size(), 55));
    out.write(name, 56);
    write_u32_le(out, positions[i].first);
    write_u32_le(out, positions[i].second);
  }
}

struct pak_fixture {
  std::filesystem::path path;
  std::filesystem::path not_a_pak;
  std::filesystem::path missing;

  pak_fixture() {
    static std::atomic<unsigned long long> counter{ 0 };
    const std::string base = "vfs9_pakfs_test_" + std::to_string(counter.fetch_add(1));
    path = std::filesystem::temp_directory_path() / (base + ".pak");
    not_a_pak = std::filesystem::temp_directory_path() / (base + ".txt");
    missing = std::filesystem::temp_directory_path() / (base + "_missing.pak");

    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(not_a_pak, ec);

    write_pak(path,
              {
                  { "readme.txt", "pak readme contents" },
                  { "sound/foo.txt", "foo" },
                  { "sound/sub/bar.txt", "bar content" },
                  { "empty.txt", "" },
                  { "win\\dir\\file.txt", "backslash" },
              });

    std::ofstream out(not_a_pak, std::ios::binary);
    out.write("NOTPAK!!", 8);
  }

  ~pak_fixture() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(not_a_pak, ec);
  }

  pak_fixture(const pak_fixture&) = delete;
  pak_fixture& operator=(const pak_fixture&) = delete;
};

std::string to_string(const std::vector<std::byte>& data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace

TEST_CASE_METHOD(pak_fixture, "pakfs init validates the archive path", "[pakfs]") {
  vfs9::fs::pak::fs_impl fs;
  CHECK(fs.name() == "pak");

  vfs9::fs::pak::fs_impl other;
  CHECK_FALSE(other.init(pakfs{ "relative.pak" }).has_value());
  CHECK_FALSE(other.init(pakfs{ missing }).has_value());
  CHECK_FALSE(other.init(pakfs{ std::filesystem::temp_directory_path() }).has_value()); // a directory
  CHECK_FALSE(other.init(pakfs{ not_a_pak }).has_value());                              // not a pak file

  REQUIRE(fs.init(pakfs{ path }).has_value());
  CHECK(fs.status("/") == node_status::directory);
}

TEST_CASE_METHOD(pak_fixture, "pakfs exposes the archive tree", "[pakfs]") {
  vfs9::fs::pak::fs_impl fs;
  REQUIRE(fs.init(pakfs{ path }).has_value());

  CHECK(fs.exists("/"));
  CHECK(fs.exists("/readme.txt"));
  CHECK(fs.exists("/sound"));
  CHECK(fs.exists("/sound/foo.txt"));
  CHECK(fs.exists("/sound/sub/bar.txt"));
  CHECK(fs.exists("/empty.txt"));
  CHECK(fs.exists("/win/dir/file.txt")); // backslashes are normalized
  CHECK_FALSE(fs.exists("/missing"));

  CHECK(fs.exists("/sound", node_type::directory));
  CHECK(fs.exists("/sound/foo.txt", node_type::file));
  CHECK_FALSE(fs.exists("/sound/foo.txt", node_type::directory));
  CHECK(fs.exists("/sound", "foo.txt"));
  CHECK(fs.exists("/sound", "foo.txt", node_type::file));

  std::filesystem::path remainder("sub/bar.txt");
  auto begin = remainder.begin();
  auto end = remainder.end();
  CHECK(fs.exists("/sound", begin, end));
  CHECK(fs.exists("/sound", begin, end, node_type::file));

  CHECK(fs.status("/sound/sub") == node_status::directory);
  CHECK(fs.status("/readme.txt") == node_status::file);
  CHECK(fs.status("/missing") == node_status::not_found);

  std::vector<node_entry> root_entries;
  REQUIRE(fs.list("/", root_entries).has_value());
  CHECK(root_entries.size() == 4); // readme.txt, sound, empty.txt, win

  std::vector<node_entry> sound_entries;
  REQUIRE(fs.list("/sound", sound_entries).has_value());
  CHECK(sound_entries.size() == 2); // foo.txt, sub
  CHECK_FALSE(fs.list("/readme.txt", sound_entries).has_value()); // not a dir
  CHECK_FALSE(fs.list("/missing", sound_entries).has_value());

  auto info = fs.get_info("/sound/sub/bar.txt");
  CHECK(info.status == node_status::file);
  CHECK(info.name == "bar.txt");
  CHECK(info.fs.name == "pak");
  CHECK(info.fs.path == std::filesystem::path("/sound/sub/bar.txt"));
  REQUIRE(info.file.has_value());
  CHECK(info.file->type == file_type::regular);
  CHECK(info.file->size == std::string_view("bar content").size());
  CHECK(fs.get_info("/missing").status == node_status::not_found);
}

TEST_CASE_METHOD(pak_fixture, "pakfs read-only operations are unsupported", "[pakfs]") {
  vfs9::fs::pak::fs_impl fs;
  REQUIRE(fs.init(pakfs{ path }).has_value());

  CHECK_FALSE(fs.create_directory("/new").has_value());
  CHECK_FALSE(fs.create_directory("/new", true).has_value());
  CHECK_FALSE(fs.create_file("/f", placeholder_file_create_spec{}).has_value());
  CHECK_FALSE(fs.create_file("/f", regular_file_create_spec{}).has_value());
  CHECK_FALSE(fs.create_file("/f", scalar_file_create_spec{}).has_value());
  CHECK_FALSE(fs.remove("/readme.txt").has_value());

  CHECK_FALSE(fs.open_file("/readme.txt", mapped_file_open_options{}).has_value());
  CHECK_FALSE(fs.open_file("/readme.txt", scalar_file_open_options{}).has_value());
  CHECK_FALSE(fs.open_file("/readme.txt", control_file_open_options{}).has_value());
}

TEST_CASE_METHOD(pak_fixture, "pakfs stream file reads entry content", "[pakfs]") {
  vfs9::fs::pak::fs_impl fs;
  REQUIRE(fs.init(pakfs{ path }).has_value());

  auto file = fs.open_file("/readme.txt", stream_file_open_options{});
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  const std::string_view expected = "pak readme contents";
  CHECK(stream.max_size() == expected.size());

  std::vector<std::byte> buffer(expected.size());
  std::array<mutable_buffer, 1> buffers{ mutable_buffer(buffer) };
  CHECK(stream.read(0, buffers).value() == expected.size());
  CHECK(to_string(buffer) == expected);

  // Random access read at an offset
  std::vector<std::byte> slice(4);
  std::array<mutable_buffer, 1> slice_buffers{ mutable_buffer(slice) };
  CHECK(stream.read(4, slice_buffers).value() == 4);
  CHECK(to_string(slice) == "read");

  // Writing is unsupported
  std::array<const_buffer, 1> write_buffers{ const_buffer{} };
  CHECK_FALSE(stream.write(0, write_buffers).has_value());
}

TEST_CASE_METHOD(pak_fixture, "pakfs stream reads never cross the entry boundary", "[pakfs]") {
  vfs9::fs::pak::fs_impl fs;
  REQUIRE(fs.init(pakfs{ path }).has_value());

  // "sound/foo.txt" is 3 bytes and is followed by another entry in the archive
  auto file = fs.open_file("/sound/foo.txt", stream_file_open_options{});
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  std::vector<std::byte> buffer(10, std::byte{ 'X' });
  std::array<mutable_buffer, 1> buffers{ mutable_buffer(buffer) };
  CHECK(stream.read(0, buffers).value() == 3);
  CHECK(to_string(buffer).substr(0, 3) == "foo");
  CHECK(buffer[3] == std::byte{ 'X' }); // untouched, read was clamped
}

TEST_CASE_METHOD(pak_fixture, "pakfs seq stream reads and seeks", "[pakfs]") {
  vfs9::fs::pak::fs_impl fs;
  REQUIRE(fs.init(pakfs{ path }).has_value());
  auto file = fs.open_file("/sound/sub/bar.txt", seq_stream_file_open_options{});
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  const std::string_view expected = "bar content";
  CHECK(stream.seekable());
  CHECK(stream.max_size() == expected.size());

  std::vector<std::byte> small(4);
  std::array<mutable_buffer, 1> small_buffers{ mutable_buffer(small) };
  CHECK(stream.read(small_buffers).value() == 4);
  CHECK(stream.tell_read() == 4);
  CHECK(to_string(small) == "bar ");

  // Rewind to the beginning
  REQUIRE(stream.seek_read(0, seq_stream_file::seek_whence::begin).has_value());
  std::vector<std::byte> full(expected.size());
  std::array<mutable_buffer, 1> full_buffers{ mutable_buffer(full) };
  CHECK(stream.read(full_buffers).value() == expected.size());
  CHECK(to_string(full) == expected);

  // Seek from the end
  REQUIRE(stream.seek_read(-7, seq_stream_file::seek_whence::end).has_value());
  std::vector<std::byte> tail(7);
  std::array<mutable_buffer, 1> tail_buffers{ mutable_buffer(tail) };
  CHECK(stream.read(tail_buffers).value() == 7);
  CHECK(to_string(tail) == "content");

  // Seek out of range / writing is unsupported
  CHECK_FALSE(stream.seek_read(-1, seq_stream_file::seek_whence::begin).has_value());
  CHECK_FALSE(stream.seek_write(0, seq_stream_file::seek_whence::begin).has_value());
  std::array<const_buffer, 1> const_buffers{ const_buffer{} };
  CHECK_FALSE(stream.write(const_buffers).has_value());
}

TEST_CASE_METHOD(pak_fixture, "pakfs integrates with the namespace", "[pakfs]") {
  auto ns = make_namespace();
  REQUIRE(ns->create_directory("/p").has_value());
  REQUIRE(ns->mount("/p", merge_flag::replace, true, pakfs{ path }).has_value());

  CHECK(ns->status("/p/sound/sub/bar.txt").value() == node_status::file);
  CHECK(ns->get_info("/p/sound/foo.txt").value().fs.name == "pak");

  auto entries = ns->list("/p/sound");
  REQUIRE(entries.has_value());
  CHECK(entries->size() == 2);

  std::vector<std::byte> buffer(4);
  std::array<mutable_buffer, 1> buffers{ mutable_buffer(buffer) };
  CHECK(ns->read_all("/p/sound/sub/bar.txt", buffers, {}).value() == 4);
  CHECK(to_string(buffer) == "bar ");

  // Mount the archive at the root as well
  auto root_ns = make_namespace();
  REQUIRE(root_ns->mount("/", merge_flag::replace, true, pakfs{ path }).has_value());
  CHECK(root_ns->status("/readme.txt").value() == node_status::file);
}
