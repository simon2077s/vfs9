/**
 * @file memfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Memory file system unit test cases
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vfs9/memfs/memfs.hpp"

#include <any>
#include <array>
#include <cstring>
#include <filesystem>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "vfs9/error.hpp"

namespace {

using namespace vfs9;

struct memfs_fixture {
  memfs_impl::fs_impl fs;
  memfs_fixture() { REQUIRE(fs.init(memfs{}).has_value()); }
};

} // namespace

TEST_CASE_METHOD(memfs_fixture, "memfs root exists and is a directory", "[memfs]") {
  CHECK(fs.exists("/"));
  CHECK(fs.exists("/", node_type::directory));
  CHECK(fs.status("/") == node_status::directory);
  CHECK(fs.get_info("/").status == node_status::directory);
  CHECK(fs.get_info("/").name == "/");
  CHECK(fs.get_info("/").fs.name == "memfs");
}

TEST_CASE_METHOD(memfs_fixture, "memfs create_directory", "[memfs]") {
  CHECK(fs.create_directory("/a").has_value());
  CHECK(fs.status("/a") == node_status::directory);
  CHECK(fs.create_directory("/a/b", true).has_value());
  CHECK(fs.exists("/a/b"));

  CHECK_FALSE(fs.create_directory("/a").has_value());
  CHECK_FALSE(fs.create_directory("/missing/child").has_value());
}

TEST_CASE_METHOD(memfs_fixture, "memfs create_file", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());

  CHECK(fs.create_file("/a/p", placeholder_file_create_spec{}).has_value());
  CHECK(fs.status("/a/p") == node_status::file);
  CHECK(fs.get_info("/a/p").file->type == file_type::placeholder);

  regular_file_create_spec regular_spec;
  regular_spec.initial_size = 16;
  CHECK(fs.create_file("/a/f", regular_spec).has_value());
  CHECK(fs.get_info("/a/f").file->type == file_type::regular);
  CHECK(fs.get_info("/a/f").file->size == 16);

  scalar_file_create_spec scalar_spec;
  scalar_spec.init_value = std::any(42);
  CHECK(fs.create_file("/a/s", scalar_spec).has_value());
  CHECK(fs.get_info("/a/s").file->type == file_type::scalar);
  CHECK(*fs.get_info("/a/s").file->scalar_type == typeid(int));

  CHECK_FALSE(fs.create_file("/a/f", regular_spec).has_value());
  CHECK_FALSE(fs.create_file("/missing/f", regular_spec).has_value());
}

TEST_CASE_METHOD(memfs_fixture, "memfs exists overloads", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());
  REQUIRE(fs.create_directory("/a/d").has_value());

  CHECK(fs.exists("/a", "f"));
  CHECK(fs.exists("/a", "d"));
  CHECK_FALSE(fs.exists("/a", "missing"));

  CHECK(fs.exists("/a", "f", node_type::file));
  CHECK_FALSE(fs.exists("/a", "f", node_type::directory));
  CHECK(fs.exists("/a", "d", node_type::directory));

  std::filesystem::path remainder("f");
  auto begin = remainder.begin();
  auto end = remainder.end();
  CHECK(fs.exists("/a", begin, end));
  CHECK(fs.exists("/a", begin, end, node_type::file));
  CHECK_FALSE(fs.exists("/a", begin, end, node_type::directory));
}

TEST_CASE_METHOD(memfs_fixture, "memfs list", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());
  REQUIRE(fs.create_directory("/a/d").has_value());

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

  CHECK_FALSE(fs.list("/a/f", entries).has_value());
  CHECK_FALSE(fs.list("/missing", entries).has_value());
}

TEST_CASE_METHOD(memfs_fixture, "memfs remove", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());

  CHECK_FALSE(fs.remove("/missing").has_value());
  CHECK_FALSE(fs.remove("/a").has_value());
  CHECK(fs.remove("/a/f").has_value());
  CHECK_FALSE(fs.exists("/a/f"));
  CHECK(fs.remove("/a").has_value());
  CHECK_FALSE(fs.exists("/a"));

  REQUIRE(fs.create_directory("/x/y/z", true).has_value());
  CHECK(fs.remove("/x", true).has_value());
  CHECK_FALSE(fs.exists("/x"));
}

TEST_CASE_METHOD(memfs_fixture, "memfs stream file read and write", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  REQUIRE(fs.create_file("/a/f", regular_file_create_spec{}).has_value());

  auto file = fs.open_file("/a/f", stream_file_open_options{});
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
}

TEST_CASE_METHOD(memfs_fixture, "memfs mapped file exposes data", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  regular_file_create_spec spec;
  spec.initial_size = 8;
  REQUIRE(fs.create_file("/a/m", spec).has_value());

  auto file = fs.open_file("/a/m", mapped_file_open_options{});
  REQUIRE(file.has_value());
  auto& mapped = *file.value();

  auto data = mapped.data();
  REQUIRE(data.size() == 8);
  std::memset(data.data(), 0x7f, data.size());
  const auto& const_mapped = mapped;
  CHECK(const_mapped.data()[0] == std::byte{ 0x7f });
  CHECK(mapped.reservation() >= 8);
  CHECK(mapped.starting_offset() == 0);
}

TEST_CASE_METHOD(memfs_fixture, "memfs scalar file read and write", "[memfs]") {
  REQUIRE(fs.create_directory("/a").has_value());
  scalar_file_create_spec scalar_spec;
  scalar_spec.init_value = std::any(42);
  REQUIRE(fs.create_file("/a/s", scalar_spec).has_value());

  auto file = fs.open_file("/a/s", scalar_file_open_options{});
  REQUIRE(file.has_value());
  auto& scalar = *file.value();

  CHECK(scalar.type() == typeid(int));
  CHECK(std::any_cast<int>(scalar.read().value()) == 42);

  CHECK(scalar.write(std::any(100)).has_value());
  CHECK(std::any_cast<int>(scalar.read().value()) == 100);
}

TEST_CASE_METHOD(memfs_fixture, "memfs control file is unsupported", "[memfs]") {
  REQUIRE(fs.create_file("/c", placeholder_file_create_spec{}).has_value());
  CHECK_FALSE(fs.open_file("/c", control_file_open_options{}).has_value());
}

TEST_CASE_METHOD(memfs_fixture, "memfs refuses to remove an open file", "[memfs]") {
  REQUIRE(fs.create_directory("/d").has_value());
  REQUIRE(fs.create_file("/d/f", regular_file_create_spec{}).has_value());

  auto file = fs.open_file("/d/f", stream_file_open_options{});
  REQUIRE(file.has_value());

  // Removing the open file (or its parent directory) must be refused
  auto removed_file = fs.remove("/d/f");
  REQUIRE_FALSE(removed_file.has_value());
  CHECK(removed_file.error() == errc::file_in_use);

  auto removed_dir = fs.remove("/d", true);
  REQUIRE_FALSE(removed_dir.has_value());
  CHECK(removed_dir.error() == errc::file_in_use);

  // Once the handle is closed the file can be removed
  file.value().reset();
  CHECK(fs.remove("/d/f").has_value());
  CHECK_FALSE(fs.exists("/d/f"));
}
