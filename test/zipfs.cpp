/**
 * @file zipfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Zip file system unit test cases
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/zip/fs.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include "vfs9/vns.hpp"

namespace {

using namespace vfs9;

/**
 * @brief Add an entry to a zip archive being written
 *
 */
void add_zip_entry(void* writer, const std::string& name, std::string_view data, bool is_dir = false,
                   uint16_t method = MZ_COMPRESS_METHOD_STORE) {
  mz_zip_file info{};
  info.filename = name.c_str();
  info.version_madeby = 0; // MSDOS host system
  info.external_fa = is_dir ? 0x10u : 0u;
  info.compression_method = method;
  REQUIRE(mz_zip_writer_entry_open(writer, &info) == MZ_OK);
  if (!data.empty()) {
    const int32_t written = mz_zip_writer_entry_write(writer, data.data(), static_cast<int32_t>(data.size()));
    REQUIRE(written == static_cast<int32_t>(data.size()));
  }
  REQUIRE(mz_zip_writer_entry_close(writer) == MZ_OK);
}

/**
 * @brief Build a temporary zip archive with a known content layout
 *
 */
struct zip_fixture {
  std::filesystem::path path;

  zip_fixture() {
    static std::atomic<unsigned long long> counter{ 0 };
    const std::string name = "vfs9_zipfs_test_" + std::to_string(counter.fetch_add(1)) + ".zip";
    path = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove(path, ec);

    void* writer = mz_zip_writer_create();
    REQUIRE(writer != nullptr);
    REQUIRE(mz_zip_writer_open_file(writer, path.c_str(), 0, 0) == MZ_OK);

    add_zip_entry(writer, "readme.txt", "vfs9 zipfs store");
    add_zip_entry(writer, "dir/a.txt", "deflate content of a", false, MZ_COMPRESS_METHOD_DEFLATE);
    add_zip_entry(writer, "dir/sub/b.txt", "b", false, MZ_COMPRESS_METHOD_DEFLATE);
    add_zip_entry(writer, "empty.txt", "", false, MZ_COMPRESS_METHOD_DEFLATE);
    add_zip_entry(writer, "emptydir/", "", true);
    add_zip_entry(writer, "文本.txt", "utf8");

    REQUIRE(mz_zip_writer_close(writer) == MZ_OK);
    mz_zip_writer_delete(&writer);
  }

  ~zip_fixture() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }

  zip_fixture(const zip_fixture&) = delete;
  zip_fixture& operator=(const zip_fixture&) = delete;
};

std::string to_string(const std::vector<std::byte>& data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace

TEST_CASE_METHOD(zip_fixture, "zipfs init validates the archive path", "[zipfs]") {
  vfs9::fs::zip::fs_impl fs;
  CHECK(fs.name() == "zip");

  vfs9::fs::zip::fs_impl other;
  CHECK_FALSE(other.init(zipfs{ "relative.zip" }).has_value());
  CHECK_FALSE(other.init(zipfs{ std::filesystem::temp_directory_path() / "vfs9_zipfs_missing.zip" }).has_value());
  CHECK_FALSE(other.init(zipfs{ std::filesystem::temp_directory_path() }).has_value()); // a directory

  REQUIRE(fs.init(zipfs{ path }).has_value());
  CHECK(fs.status("/") == node_status::directory);
}

TEST_CASE_METHOD(zip_fixture, "zipfs exposes the archive tree", "[zipfs]") {
  vfs9::fs::zip::fs_impl fs;
  REQUIRE(fs.init(zipfs{ path }).has_value());

  CHECK(fs.exists("/"));
  CHECK(fs.exists("/dir"));
  CHECK(fs.exists("/dir/a.txt"));
  CHECK(fs.exists("/dir/sub/b.txt"));
  CHECK(fs.exists("/emptydir"));
  CHECK(fs.exists("/文本.txt"));
  CHECK_FALSE(fs.exists("/missing"));

  CHECK(fs.exists("/dir", node_type::directory));
  CHECK(fs.exists("/dir/a.txt", node_type::file));
  CHECK_FALSE(fs.exists("/dir/a.txt", node_type::directory));
  CHECK(fs.exists("/dir", "a.txt"));
  CHECK(fs.exists("/dir", "a.txt", node_type::file));
  CHECK_FALSE(fs.exists("/dir", "a.txt", node_type::directory));

  std::filesystem::path remainder("sub/b.txt");
  auto begin = remainder.begin();
  auto end = remainder.end();
  CHECK(fs.exists("/dir", begin, end));
  CHECK(fs.exists("/dir", begin, end, node_type::file));

  CHECK(fs.status("/") == node_status::directory);
  CHECK(fs.status("/dir/sub") == node_status::directory);
  CHECK(fs.status("/readme.txt") == node_status::file);
  CHECK(fs.status("/missing") == node_status::not_found);

  auto root_entries = [&fs]() {
    std::vector<node_entry> out;
    REQUIRE(fs.list("/", out).has_value());
    return out;
  }();
  CHECK(root_entries.size() == 5); // readme.txt, dir, empty.txt, emptydir, 文本.txt
  bool has_readme = false;
  bool has_dir = false;
  bool has_emptydir = false;
  for (const auto& entry : root_entries) {
    if (entry.name == "readme.txt") {
      has_readme = entry.type == node_type::file && entry.ft == file_type::regular;
    } else if (entry.name == "dir") {
      has_dir = entry.type == node_type::directory;
    } else if (entry.name == "emptydir") {
      has_emptydir = entry.type == node_type::directory;
    }
  }
  CHECK(has_readme);
  CHECK(has_dir);
  CHECK(has_emptydir);

  std::vector<node_entry> dir_entries;
  REQUIRE(fs.list("/dir", dir_entries).has_value());
  REQUIRE(dir_entries.size() == 2); // a.txt, sub
  CHECK_FALSE(fs.list("/readme.txt", dir_entries).has_value());
  CHECK_FALSE(fs.list("/missing", dir_entries).has_value());

  auto info = fs.get_info("/dir/a.txt");
  CHECK(info.status == node_status::file);
  CHECK(info.name == "a.txt");
  CHECK(info.fs.name == "zip");
  CHECK(info.fs.path == std::filesystem::path("/dir/a.txt"));
  REQUIRE(info.file.has_value());
  CHECK(info.file->type == file_type::regular);
  CHECK(info.file->size == std::string_view("deflate content of a").size());
  CHECK(fs.get_info("/missing").status == node_status::not_found);
}

TEST_CASE_METHOD(zip_fixture, "zipfs refuses to remove an open file", "[zipfs]") {
  vfs9::fs::zip::fs_impl fs;
  REQUIRE(fs.init(zipfs{ path }).has_value());

  auto file = fs.open_file("/readme.txt", seq_stream_file_open_options{});
  REQUIRE(file.has_value());

  // zipfs is read-only, so any removal (open or not) is refused
  auto removed = fs.remove("/readme.txt");
  REQUIRE_FALSE(removed.has_value());
  CHECK(removed.error() == errc::unsupported);
}

TEST_CASE_METHOD(zip_fixture, "zipfs read-only operations are unsupported", "[zipfs]") {
  vfs9::fs::zip::fs_impl fs;
  REQUIRE(fs.init(zipfs{ path }).has_value());

  CHECK_FALSE(fs.create_directory("/new").has_value());
  CHECK_FALSE(fs.create_directory("/new", true).has_value());
  CHECK_FALSE(fs.create_file("/f", placeholder_file_create_spec{}).has_value());
  CHECK_FALSE(fs.create_file("/f", regular_file_create_spec{}).has_value());
  CHECK_FALSE(fs.create_file("/f", scalar_file_create_spec{}).has_value());
  CHECK_FALSE(fs.remove("/readme.txt").has_value());
  CHECK_FALSE(fs.remove("/dir", true).has_value());

  CHECK_FALSE(fs.open_file("/readme.txt", stream_file_open_options{}).has_value());
  CHECK_FALSE(fs.open_file("/readme.txt", mapped_file_open_options{}).has_value());
  CHECK_FALSE(fs.open_file("/readme.txt", scalar_file_open_options{}).has_value());
  CHECK_FALSE(fs.open_file("/readme.txt", control_file_open_options{}).has_value());
}

TEST_CASE_METHOD(zip_fixture, "zipfs seq stream reads entry content", "[zipfs]") {
  vfs9::fs::zip::fs_impl fs;
  REQUIRE(fs.init(zipfs{ path }).has_value());

  auto file = fs.open_file("/dir/a.txt", seq_stream_file_open_options{});
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  const std::string_view expected = "deflate content of a";
  CHECK(stream.max_size() == expected.size());
  CHECK(stream.seekable());
  CHECK(stream.tell_read() == 0);

  std::vector<std::byte> buffer(expected.size());
  std::array<mutable_buffer, 1> buffers{ mutable_buffer(buffer) };
  CHECK(stream.read(buffers).value() == expected.size());
  CHECK(stream.tell_read() == expected.size());
  CHECK(to_string(buffer) == expected);

  // Reading at EOF returns zero
  CHECK(stream.read(buffers).value() == 0);

  // Writing is unsupported
  std::array<const_buffer, 1> write_buffers{ const_buffer{} };
  CHECK_FALSE(stream.write(write_buffers).has_value());
}

TEST_CASE_METHOD(zip_fixture, "zipfs seq stream seek behavior", "[zipfs]") {
  vfs9::fs::zip::fs_impl fs;
  REQUIRE(fs.init(zipfs{ path }).has_value());
  auto file = fs.open_file("/readme.txt", seq_stream_file_open_options{});
  REQUIRE(file.has_value());
  auto& stream = *file.value();

  const std::string_view expected = "vfs9 zipfs store";

  // Partial read, then rewind via begin and read the whole entry
  std::vector<std::byte> small(4);
  std::array<mutable_buffer, 1> small_buffers{ mutable_buffer(small) };
  CHECK(stream.read(small_buffers).value() == 4);
  CHECK(stream.tell_read() == 4);

  std::vector<std::byte> full(expected.size());
  std::array<mutable_buffer, 1> full_buffers{ mutable_buffer(full) };
  REQUIRE(stream.seek_read(0, seq_stream_file::seek_whence::begin).has_value());
  CHECK(stream.tell_read() == 0);
  CHECK(stream.read(full_buffers).value() == expected.size());
  CHECK(to_string(full) == expected);

  // Backward relative seek re-opens the entry
  REQUIRE(stream.seek_read(-2, seq_stream_file::seek_whence::current).has_value());
  std::vector<std::byte> tail(2);
  std::array<mutable_buffer, 1> tail_buffers{ mutable_buffer(tail) };
  CHECK(stream.read(tail_buffers).value() == 2);
  CHECK(to_string(tail) == expected.substr(expected.size() - 2));

  // Seek to end, then read nothing
  REQUIRE(stream.seek_read(0, seq_stream_file::seek_whence::end).has_value());
  CHECK(stream.tell_read() == expected.size());
  CHECK(stream.read(full_buffers).value() == 0);

  // Seeking out of range / writing are unsupported
  CHECK_FALSE(stream.seek_read(1, seq_stream_file::seek_whence::end).has_value());
  CHECK_FALSE(stream.seek_read(-1, seq_stream_file::seek_whence::begin).has_value());
  CHECK_FALSE(stream.seek_write(0, seq_stream_file::seek_whence::begin).has_value());
}

TEST_CASE_METHOD(zip_fixture, "zipfs integrates with the namespace", "[zipfs]") {
  auto ns = make_namespace();
  REQUIRE(ns->create_directory("/z").has_value());
  REQUIRE(ns->mount("/z", merge_flag::replace, true, zipfs{ path }).has_value());

  CHECK(ns->status("/z/dir/sub/b.txt").value() == node_status::file);
  CHECK(ns->get_info("/z/dir/a.txt").value().fs.name == "zip");

  auto entries = ns->list("/z/dir");
  REQUIRE(entries.has_value());
  CHECK(entries->size() == 2);

  std::vector<std::byte> buffer(1);
  std::array<mutable_buffer, 1> buffers{ mutable_buffer(buffer) };
  CHECK(ns->read_all("/z/dir/sub/b.txt", buffers, {}).value() == 1);
  CHECK(to_string(buffer) == "b");

  // Mount the archive as the root as well
  auto root_ns = make_namespace();
  REQUIRE(root_ns->mount("/", merge_flag::replace, true, zipfs{ path }).has_value());
  CHECK(root_ns->status("/readme.txt").value() == node_status::file);
}
