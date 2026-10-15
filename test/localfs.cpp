/**
 * @file localfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Local file system unit test cases
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vfs9/fs/local/fs.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "vfs9/error.hpp"

namespace {

using namespace vfs9;

std::filesystem::path unique_root() {
  static std::atomic<unsigned long long> counter{ 0 };
  return std::filesystem::temp_directory_path() / ("vfs9_localfs_test_" + std::to_string(counter.fetch_add(1)));
}

struct localfs_fixture {
  std::filesystem::path root;
  vfs9::fs::local::fs_impl fs;

  localfs_fixture() {
    root = unique_root();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directory(root, ec);
    REQUIRE(fs.init(localfs{ root }).has_value());
  }

  ~localfs_fixture() {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }
};

} // namespace

TEST_CASE_METHOD(localfs_fixture, "localfs init requires an absolute directory", "[localfs]") {
  CHECK(fs.name() == "local");

  vfs9::fs::local::fs_impl other;
  CHECK_FALSE(other.init(localfs{ "relative/path" }).has_value());
  CHECK_FALSE(other.init(localfs{ root / "missing" }).has_value());

  std::filesystem::path a_file = root / "plain_file";
  {
    std::ofstream ofs(a_file);
    ofs << "x";
  }
  CHECK_FALSE(other.init(localfs{ a_file }).has_value());
}

TEST_CASE_METHOD(localfs_fixture, "localfs create_directory and remove", "[localfs]") {
  CHECK(fs.create_directory("/a").has_value());
  CHECK(fs.status("/a") == node_status::directory);
  CHECK(fs.create_directory("/a/b", true).has_value());
  CHECK(fs.exists("/a/b"));

  CHECK_FALSE(fs.create_directory("/a").has_value());
  CHECK_FALSE(fs.create_directory("/missing/child").has_value());

  CHECK_FALSE(fs.remove("/a").has_value());
  CHECK(fs.remove("/a", true).has_value());
  CHECK_FALSE(fs.exists("/a"));
}

TEST_CASE_METHOD(localfs_fixture, "localfs create_file", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());

  CHECK(fs.create_file("/a/p", placeholder_file_create_spec{}).has_value());
  CHECK(fs.status("/a/p") == node_status::file);
  CHECK(fs.get_info("/a/p").file->size == 0);

  regular_file_create_spec spec;
  spec.initial_size = 16;
  CHECK(fs.create_file("/a/f", spec).has_value());
  CHECK(fs.get_info("/a/f").file->type == file_type::regular);
  CHECK(fs.get_info("/a/f").file->size == 16);

  CHECK_FALSE(fs.create_file("/a/f", spec).has_value());
  CHECK_FALSE(fs.create_file("/missing/f", spec).has_value());

  scalar_file_create_spec scalar_spec;
  CHECK_FALSE(fs.create_file("/a/s", scalar_spec).has_value());
}

TEST_CASE_METHOD(localfs_fixture, "localfs exists and list filter non-regular files", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());
  REQUIRE(fs.create_directory("/a/d").has_value());

  CHECK(fs.exists("/a", "f"));
  CHECK(fs.exists("/a", "f", node_type::file));
  CHECK_FALSE(fs.exists("/a", "f", node_type::directory));
  CHECK(fs.exists("/a", "d", node_type::directory));

  std::filesystem::path remainder("f");
  auto begin = remainder.begin();
  auto end = remainder.end();
  CHECK(fs.exists("/a", begin, end));
  CHECK(fs.exists("/a", begin, end, node_type::file));

  std::vector<node_entry> entries;
  CHECK(fs.list("/a", entries).has_value());
  REQUIRE(entries.size() == 2);
  bool has_f = false;
  bool has_d = false;
  for (const auto& entry : entries) {
    if (entry.name == "f") {
      has_f = true;
      CHECK(entry.type == node_type::file);
      CHECK(entry.ft == file_type::regular);
    } else if (entry.name == "d") {
      has_d = true;
      CHECK(entry.type == node_type::directory);
    }
  }
  CHECK(has_f);
  CHECK(has_d);
}

TEST_CASE_METHOD(localfs_fixture, "localfs stream file read and write", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());

  stream_file_open_options stream_options;
  stream_options.mode = file_open_mode::write;
  auto file = fs.open_file("/a/f", stream_options);
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  std::array<std::byte, 5> in{};
  std::memcpy(in.data(), "hello", 5);
  std::array<const_buffer, 1> write_buffers{ const_buffer(in) };
  CHECK(stream.write(0, write_buffers).value() == 5);

  std::array<std::byte, 5> out{};
  std::array<mutable_buffer, 1> read_buffers{ mutable_buffer(out) };
  CHECK(stream.read(0, read_buffers).value() == 5);
  CHECK(std::memcmp(out.data(), "hello", 5) == 0);
  CHECK(stream.max_size() == 5);
  CHECK(fs.get_info("/a/f").file->size == 5);
}

TEST_CASE_METHOD(localfs_fixture, "localfs seq stream file read and write", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());

  seq_stream_file_open_options seq_options;
  seq_options.mode = file_open_mode::write;
  auto file = fs.open_file("/a/f", seq_options);
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  std::array<std::byte, 3> in{};
  std::memcpy(in.data(), "abc", 3);
  std::array<const_buffer, 1> write_buffers{ const_buffer(in) };
  CHECK(stream.write(write_buffers).value() == 3);
  CHECK(stream.tell_write() == 3);

  CHECK(stream.seek_read(0, seq_stream_file::seek_whence::begin).has_value());
  std::array<std::byte, 3> out{};
  std::array<mutable_buffer, 1> read_buffers{ mutable_buffer(out) };
  CHECK(stream.read(read_buffers).value() == 3);
  CHECK(std::memcmp(out.data(), "abc", 3) == 0);
  CHECK(stream.tell_read() == 3);
}

TEST_CASE_METHOD(localfs_fixture, "localfs mapped file exposes data", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  regular_file_create_spec spec;
  spec.initial_size = 8;
  REQUIRE(fs.create_file("/a/m", spec).has_value());

  mapped_file_open_options mapped_options;
  mapped_options.mode = file_open_mode::write;
  mapped_options.reservation = 16;
  auto file = fs.open_file("/a/m", mapped_options);
  REQUIRE(file.has_value());
  auto& mapped = *file.value();

  auto data = mapped.data();
  REQUIRE(data.size() == 8);
  std::memset(data.data(), 0x7f, data.size());
  const auto& const_mapped = mapped;
  CHECK(const_mapped.data()[0] == std::byte{ 0x7f });
  CHECK(mapped.reservation() >= 8);
  CHECK(mapped.starting_offset() == 0);
  CHECK(mapped.max_size() == 8);
}

TEST_CASE_METHOD(localfs_fixture, "localfs scalar and control are unsupported", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());

  scalar_file_open_options scalar_options;
  CHECK_FALSE(fs.open_file("/a/f", scalar_options).has_value());

  control_file_open_options control_options;
  CHECK_FALSE(fs.open_file("/a/f", control_options).has_value());
}

TEST_CASE_METHOD(localfs_fixture, "localfs refuses to remove an open file", "[localfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());

  stream_file_open_options options;
  options.mode = file_open_mode::write;
  auto file = fs.open_file("/a/f", options);
  REQUIRE(file.has_value());

  // Removing the open file (or its parent directory) must be refused
  auto removed_file = fs.remove("/a/f");
  REQUIRE_FALSE(removed_file.has_value());
  CHECK(removed_file.error() == errc::file_in_use);

  auto removed_dir = fs.remove("/a", true);
  REQUIRE_FALSE(removed_dir.has_value());
  CHECK(removed_dir.error() == errc::file_in_use);

  // Once the handle is closed the file can be removed
  file.value().reset();
  CHECK(fs.remove("/a", true).has_value());
  CHECK_FALSE(fs.exists("/a"));
}
