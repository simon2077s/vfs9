/**
 * @file vns.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Virtual namespace unit test cases
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vfs9/vns.hpp"

#include <any>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "vfs9/localfs/localfs.hpp"

namespace {

using namespace vfs9;

/**
 * @brief Temporary directory tree used as the backing store of localfs
 *
 */
struct tree_builder {
  std::filesystem::path root;

  tree_builder() {
    static std::atomic<unsigned long long> counter{ 0 };
    root = std::filesystem::temp_directory_path() / ("vfs9_vns_test_" + std::to_string(counter.fetch_add(1)));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
  }

  ~tree_builder() {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }

  tree_builder(const tree_builder&) = delete;
  tree_builder& operator=(const tree_builder&) = delete;

  tree_builder& dir(const std::filesystem::path& rel) {
    std::error_code ec;
    std::filesystem::create_directories(root / rel, ec);
    return *this;
  }

  tree_builder& file(const std::filesystem::path& rel, std::string_view content = "vfs9") {
    std::ofstream ofs(root / rel);
    ofs << content;
    return *this;
  }
};

struct vns_fixture {
  std::shared_ptr<vnamespace> ns = make_namespace();
};

} // namespace

TEST_CASE("vns make_namespace creates distinct namespaces", "[vns]") {
  auto ns0 = make_namespace();
  auto ns1 = make_namespace();
  REQUIRE(ns0 != nullptr);
  REQUIRE(ns1 != nullptr);
  CHECK(ns0->id() != ns1->id());
  CHECK(ns0->tracer() == nullptr);
  CHECK_FALSE(ns0->is_frozen());

  auto tracer = std::make_shared<vfs9::tracer>();
  auto ns2 = make_namespace(memfs{}, tracer);
  REQUIRE(ns2 != nullptr);
  CHECK(ns2->tracer() == tracer.get());
}

TEST_CASE_METHOD(vns_fixture, "vns root is a memfs directory", "[vns]") {
  auto status = ns->status("/");
  REQUIRE(status.has_value());
  CHECK(*status == node_status::directory);

  auto exists = ns->exists("/");
  REQUIRE(exists.has_value());
  CHECK(*exists);

  auto info = ns->get_info("/");
  REQUIRE(info.has_value());
  CHECK(info->status == node_status::directory);
  CHECK(info->fs.name == "memfs");

  CHECK_FALSE(ns->exists("/missing").value());
  CHECK(ns->status("/missing").value() == node_status::not_found);
  CHECK(ns->get_info("/missing").value().status == node_status::not_found);
}

TEST_CASE_METHOD(vns_fixture, "vns query rejects relative paths", "[vns]") {
  auto status = ns->status("relative");
  REQUIRE_FALSE(status.has_value());
  CHECK(status.error() == errc::not_an_abs_path);

  auto info = ns->get_info("relative");
  REQUIRE_FALSE(info.has_value());
  CHECK(info.error() == errc::not_an_abs_path);
}

TEST_CASE_METHOD(vns_fixture, "vns mount replaces the root file system", "[vns]") {
  tree_builder tree;
  tree.dir("sub");

  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  auto info = ns->get_info("/");
  REQUIRE(info.has_value());
  CHECK(info->fs.name == "localfs");
  CHECK(info->status == node_status::directory);

  auto entries = ns->list("/");
  REQUIRE(entries.has_value());
  REQUIRE(entries->size() == 1);
  CHECK((*entries)[0].name == "sub");
  CHECK((*entries)[0].type == node_type::directory);
}

TEST_CASE_METHOD(vns_fixture, "vns resolves paths nested under a mount point", "[vns]") {
  tree_builder root_tree;
  root_tree.dir("a");

  tree_builder a_tree;
  a_tree.dir("b");
  a_tree.file("hello");

  tree_builder ab_tree;
  ab_tree.dir("deep");

  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ root_tree.root }).has_value());
  REQUIRE(ns->mount("/a", merge_flag::replace, true, localfs{ a_tree.root }).has_value());

  auto info_a = ns->get_info("/a");
  REQUIRE(info_a.has_value());
  CHECK(info_a->fs.name == "localfs");
  CHECK(info_a->status == node_status::directory);

  // Deeper mount, only reachable if the path resolution descends into the mount at /a
  REQUIRE(ns->mount("/a/b", merge_flag::replace, true, localfs{ ab_tree.root }).has_value());

  auto info_ab = ns->get_info("/a/b");
  REQUIRE(info_ab.has_value());
  CHECK(info_ab->fs.name == "localfs");
  CHECK(info_ab->status == node_status::directory);

  // A file which lives in the fs mounted at /a
  auto hello = ns->get_info("/a/hello");
  REQUIRE(hello.has_value());
  CHECK(hello->status == node_status::file);

  // A directory which lives in the fs mounted at /a/b
  auto deep = ns->get_info("/a/b/deep");
  REQUIRE(deep.has_value());
  CHECK(deep->status == node_status::directory);
}

TEST_CASE_METHOD(vns_fixture, "vns mount error cases", "[vns]") {
  tree_builder tree;
  tree.dir("d");
  tree.file("f");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  auto missing = ns->mount("/nope", merge_flag::replace, true, localfs{ tree.root });
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error() == errc::path_not_found);

  auto on_file = ns->mount("/f", merge_flag::replace, true, localfs{ tree.root });
  REQUIRE_FALSE(on_file.has_value());
  CHECK(on_file.error() == errc::not_a_dir_or_union_dir);

  auto relative = ns->mount("relative", merge_flag::replace, true, localfs{ tree.root });
  REQUIRE_FALSE(relative.has_value());
  CHECK(relative.error() == errc::not_an_abs_path);
}

TEST_CASE_METHOD(vns_fixture, "vns freeze blocks mutation", "[vns]") {
  tree_builder tree;
  tree.dir("d");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  ns->freeze();
  CHECK(ns->is_frozen());
  ns->freeze(); // Idempotent
  CHECK(ns->is_frozen());

  auto mount = ns->mount("/d", merge_flag::replace, true, localfs{ tree.root });
  REQUIRE_FALSE(mount.has_value());
  CHECK(mount.error() == errc::frozen);

  auto bind_result = ns->bind("/d", "/d", merge_flag::replace, true);
  REQUIRE_FALSE(bind_result.has_value());
  CHECK(bind_result.error() == errc::frozen);

  auto unmount = ns->unmount(std::filesystem::path("/d"));
  REQUIRE_FALSE(unmount.has_value());
  CHECK(unmount.error() == errc::frozen);
}

TEST_CASE_METHOD(vns_fixture, "vns bind creates a union directory by priority", "[vns]") {
  tree_builder base;
  base.dir("x");
  base.dir("d");
  base.file("only_base");

  tree_builder extra;
  extra.file("d"); // Same name as the base directory, higher priority
  extra.file("only_extra");

  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ base.root }).has_value());
  REQUIRE(ns->mount("/x", merge_flag::replace, true, localfs{ extra.root }).has_value());

  // Bind extra in front of the base root
  REQUIRE(ns->bind("/x", "/", merge_flag::before, true).has_value());

  auto entries = ns->list("/");
  REQUIRE(entries.has_value());
  REQUIRE(entries->size() == 4);

  bool has_d_file = false;
  bool has_x_dir = false;
  bool has_only_base = false;
  bool has_only_extra = false;
  for (const auto& entry : *entries) {
    if (entry.name == "d") {
      has_d_file = entry.type == node_type::file;
    } else if (entry.name == "x") {
      has_x_dir = entry.type == node_type::directory;
    } else if (entry.name == "only_base") {
      has_only_base = true;
    } else if (entry.name == "only_extra") {
      has_only_extra = true;
    }
  }
  CHECK(has_d_file);
  CHECK(has_x_dir);
  CHECK(has_only_base);
  CHECK(has_only_extra);

  // Remove the extra entry again by its bind source name
  auto unmounted = ns->unmount(std::string_view("/x"), std::filesystem::path("/"));
  REQUIRE(unmounted.has_value());
  CHECK(*unmounted);

  auto reverted = ns->list("/");
  REQUIRE(reverted.has_value());
  CHECK(reverted->size() == 3);
  CHECK(ns->get_info("/d").value().status == node_status::directory);
}

TEST_CASE_METHOD(vns_fixture, "vns bind error cases", "[vns]") {
  tree_builder tree;
  tree.dir("d");
  tree.file("f");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  auto mismatch = ns->bind("/f", "/d", merge_flag::replace, true);
  REQUIRE_FALSE(mismatch.has_value());
  CHECK(mismatch.error() == errc::different_node_type);

  auto missing = ns->bind("/nope", "/d", merge_flag::replace, true);
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error() == errc::path_not_found);

  auto not_dir = ns->bind("/f", "/f", merge_flag::before, true);
  REQUIRE_FALSE(not_dir.has_value());
  CHECK(not_dir.error() == errc::not_a_dir_or_union_dir);

  auto relative = ns->bind("f", "/d", merge_flag::replace, true);
  REQUIRE_FALSE(relative.has_value());
  CHECK(relative.error() == errc::not_an_abs_path);
}

TEST_CASE_METHOD(vns_fixture, "vns unmount overloads", "[vns]") {
  tree_builder tree;
  tree.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  // A path which is not a mount point isn't unmounted
  auto not_mount = ns->unmount(std::filesystem::path("/a"));
  REQUIRE(not_mount.has_value());
  CHECK_FALSE(*not_mount);

  tree_builder extra;
  REQUIRE(ns->mount("/a", merge_flag::replace, true, localfs{ extra.root }).has_value());
  CHECK(ns->get_info("/a").value().fs.name == "localfs");

  // Remove the whole mount point by path
  auto removed = ns->unmount(std::filesystem::path("/a"));
  REQUIRE(removed.has_value());
  CHECK(*removed);
  CHECK(ns->status("/a").value() == node_status::directory); // Backed by the base localfs

  // Remove the root mount, falling back to the memfs root
  auto removed_root = ns->unmount(std::filesystem::path("/"));
  REQUIRE(removed_root.has_value());
  CHECK(*removed_root);
  CHECK(ns->get_info("/").value().fs.name == "memfs");

  auto removed_again = ns->unmount(std::filesystem::path("/"));
  REQUIRE(removed_again.has_value());
  CHECK_FALSE(*removed_again);

  // unmount() removes every mount of the namespace
  tree_builder other;
  other.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ other.root }).has_value());
  REQUIRE(ns->mount("/a", merge_flag::replace, true, localfs{ extra.root }).has_value());
  REQUIRE(ns->unmount().has_value());
  CHECK(ns->status("/a").value() == node_status::not_found);
  CHECK(ns->get_info("/").value().status == node_status::directory); // memfs root
  CHECK(ns->get_info("/").value().fs.name == "memfs");
}

TEST_CASE_METHOD(vns_fixture, "vns shadow isolates mutations from the parent", "[vns]") {
  tree_builder tree;
  tree.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  auto shadow_result = ns->shadow();
  REQUIRE(shadow_result.has_value());
  auto shadow = *shadow_result;

  CHECK(shadow->status("/a").value() == node_status::directory);

  tree_builder extra;
  extra.file("only_shadow");
  REQUIRE(shadow->mount("/a", merge_flag::replace, true, localfs{ extra.root }).has_value());

  auto in_shadow = shadow->get_info("/a/only_shadow");
  REQUIRE(in_shadow.has_value());
  CHECK(in_shadow->status == node_status::file);

  // The parent must not see (nor be corrupted by) the shadow's mount
  auto in_parent = ns->get_info("/a/only_shadow");
  REQUIRE(in_parent.has_value());
  CHECK(in_parent->status == node_status::not_found);
  CHECK(ns->status("/a").value() == node_status::directory);

  auto shadow_of_shadow = shadow->shadow();
  REQUIRE_FALSE(shadow_of_shadow.has_value());
  CHECK(shadow_of_shadow.error() == errc::must_be_standalone_ns);
}

TEST_CASE_METHOD(vns_fixture, "vns shadow can't remove an inherited mount point", "[vns]") {
  tree_builder tree;
  tree.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  tree_builder sub;
  sub.file("f");
  REQUIRE(ns->mount("/a", merge_flag::replace, true, localfs{ sub.root }).has_value());

  auto shadow_result = ns->shadow();
  REQUIRE(shadow_result.has_value());
  auto shadow = *shadow_result;

  // The shadow inherits /a
  CHECK(shadow->get_info("/a/f").value().status == node_status::file);

  // It can't remove an inherited mount point (that isn't representable in its local table)
  auto removed = shadow->unmount(std::filesystem::path("/a"));
  REQUIRE_FALSE(removed.has_value());
  CHECK(removed.error() == errc::unsupported);

  // The inherited entry has no explicit name, so a by-name removal doesn't match it
  auto removed_by_name = shadow->unmount(std::string_view("/a"), std::filesystem::path("/a"));
  REQUIRE(removed_by_name.has_value());
  CHECK_FALSE(*removed_by_name);

  // Neither the shadow nor the parent is corrupted
  CHECK(shadow->get_info("/a/f").value().status == node_status::file);
  CHECK(ns->get_info("/a/f").value().status == node_status::file);
  CHECK(ns->get_info("/a").value().fs.name == "localfs");
}

TEST_CASE_METHOD(vns_fixture, "vns create and remove directory", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  CHECK(ns->status("/d").value() == node_status::directory);
  CHECK(ns->exists("/d", node_type::directory).value());
  CHECK_FALSE(ns->exists("/d", node_type::file).value());

  // Non-recursive creation requires the parent to exist
  CHECK_FALSE(ns->create_directory("/missing/child").has_value());
  // Recursive creation may materialize missing parents
  REQUIRE(ns->create_directory("/x/y/z", true).has_value());
  CHECK(ns->status("/x/y/z").value() == node_status::directory);

  // Removing a non-empty directory requires recursive
  REQUIRE(ns->create_directory("/x/child").has_value());
  CHECK_FALSE(ns->remove("/x").has_value());
  CHECK(ns->remove("/x", true).has_value());
  CHECK_FALSE(ns->exists("/x").value());

  // A missing path reports path_not_found
  auto missing = ns->remove("/nope");
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error() == errc::path_not_found);
}

TEST_CASE_METHOD(vns_fixture, "vns create directories under a mount point", "[vns]") {
  tree_builder tree;
  tree.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  REQUIRE(ns->create_directory("/a/new").has_value());
  CHECK(ns->status("/a/new").value() == node_status::directory);
  CHECK(ns->get_info("/a/new").value().fs.name == "localfs");

  // Recursive creation below a mount point delegates the missing components to the mounted fs
  REQUIRE(ns->create_directory("/a/deep/nested", true).has_value());
  CHECK(ns->status("/a/deep/nested").value() == node_status::directory);

  // The mounted file system is not writable if it was mounted read-only
  tree_builder ro;
  ro.dir("a");
  auto ro_ns = make_namespace();
  REQUIRE(ro_ns->mount("/", merge_flag::replace, false, localfs{ ro.root }).has_value());
  auto ec = ro_ns->create_directory("/a/dir");
  REQUIRE_FALSE(ec.has_value());
  CHECK(ec.error() == errc::unsupported);
}

TEST_CASE_METHOD(vns_fixture, "vns create file and get its info", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());

  REQUIRE(ns->create_file("/d/p", placeholder_file_create_spec{}).has_value());
  CHECK(ns->get_info("/d/p").value().file->type == file_type::placeholder);

  regular_file_create_spec regular_spec;
  regular_spec.initial_size = 16;
  REQUIRE(ns->create_file("/d/f", regular_spec).has_value());
  CHECK(ns->get_info("/d/f").value().file->type == file_type::regular);
  CHECK(ns->get_info("/d/f").value().file->size == 16);

  scalar_file_create_spec scalar_spec;
  scalar_spec.init_value = std::any(42);
  REQUIRE(ns->create_file("/d/s", scalar_spec).has_value());
  CHECK(ns->get_info("/d/s").value().file->type == file_type::scalar);

  CHECK_FALSE(ns->create_file("/d/f", regular_spec).has_value());
  CHECK_FALSE(ns->create_file("/d/missing/f", regular_spec).has_value());
}

TEST_CASE_METHOD(vns_fixture, "vns read and write a regular file", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  REQUIRE(ns->create_file("/d/f", regular_file_create_spec{}).has_value());

  std::array<std::byte, 5> in{};
  std::memcpy(in.data(), "hello", 5);
  std::array<const_buffer, 1> write_buffers{ const_buffer(in) };
  CHECK(ns->write_all("/d/f", write_buffers, {}).value() == 5);
  CHECK(ns->get_info("/d/f").value().file->size == 5);

  std::array<std::byte, 5> out{};
  std::array<mutable_buffer, 1> read_buffers{ mutable_buffer(out) };
  CHECK(ns->read_all("/d/f", read_buffers, {}).value() == 5);
  CHECK(std::memcmp(out.data(), "hello", 5) == 0);

  // Opening through the public open_file api works as well
  auto file = ns->open_file("/d/f", stream_file_open_options{});
  REQUIRE(file.has_value());
  CHECK(file.value()->max_size() == 5);
}

TEST_CASE_METHOD(vns_fixture, "vns read and write a scalar file", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  scalar_file_create_spec spec;
  spec.init_value = std::any(42);
  REQUIRE(ns->create_file("/d/s", spec).has_value());

  auto value = ns->read_scalar("/d/s", {});
  REQUIRE(value.has_value());
  CHECK(std::any_cast<int>(value.value()) == 42);

  REQUIRE(ns->write_scalar("/d/s", std::any(100), {}).has_value());
  CHECK(std::any_cast<int>(ns->read_scalar("/d/s", {}).value()) == 100);

  // Type mismatch is reported by the underlying file system
  CHECK_FALSE(ns->write_scalar("/d/s", std::any(std::string("bad")), {}).has_value());
}

TEST_CASE_METHOD(vns_fixture, "vns writes are dispatched to the first writable mount entry", "[vns]") {
  tree_builder writable;
  writable.dir("a");
  tree_builder readonly;
  readonly.dir("a");

  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ writable.root }).has_value());
  // Bind a read-only entry in front of the writable root
  REQUIRE(ns->mount("/a", merge_flag::replace, false, localfs{ readonly.root }).has_value());
  REQUIRE(ns->bind("/a", "/", merge_flag::before, false).has_value());

  // The union now has a non-writable entry in front of the writable root, so the write must fall
  // through to the writable one.
  REQUIRE(ns->create_directory("/created").has_value());
  CHECK(ns->status("/created").value() == node_status::directory);
  CHECK(std::filesystem::exists(writable.root / "created"));
  CHECK_FALSE(std::filesystem::exists(readonly.root / "created"));
}

TEST_CASE_METHOD(vns_fixture, "vns clone is an independent standalone namespace", "[vns]") {
  tree_builder tree;
  tree.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  auto cloned = ns->clone();
  REQUIRE(cloned != nullptr);
  CHECK(cloned->id() != ns->id());

  auto root_info = cloned->get_info("/");
  REQUIRE(root_info.has_value());
  CHECK(root_info->fs.name == "localfs");

  cloned->freeze();
  CHECK(cloned->is_frozen());
  CHECK_FALSE(ns->is_frozen());

  tree_builder extra;
  extra.file("x");
  REQUIRE(ns->mount("/a", merge_flag::replace, true, localfs{ extra.root }).has_value());
  CHECK(ns->get_info("/a/x").value().status == node_status::file);

  auto in_clone = cloned->get_info("/a/x");
  REQUIRE(in_clone.has_value());
  CHECK(in_clone->status == node_status::not_found);

  // Cloning a shadow materializes the inherited mount table into a standalone namespace
  auto shadow_result = ns->shadow();
  REQUIRE(shadow_result.has_value());
  auto shadow_clone = (*shadow_result)->clone();
  REQUIRE(shadow_clone != nullptr);
  CHECK(shadow_clone->is_frozen() == false);
  auto shadow_root = shadow_clone->get_info("/");
  REQUIRE(shadow_root.has_value());
  CHECK(shadow_root->fs.name == "localfs");
}

// ---------------------------------------------------------------------------
// unmount overloads / union teardown
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns unmount by source path and edge cases", "[vns]") {
  tree_builder base;
  base.dir("src");
  base.dir("dst");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ base.root }).has_value());

  REQUIRE(ns->bind("/src", "/dst", merge_flag::replace, true).has_value());
  CHECK(ns->get_info("/dst").value().fs.name == "localfs");

  // unmount(new_path, old_path) removes the bind by its source path
  auto removed = ns->unmount(std::filesystem::path("/src"), std::filesystem::path("/dst"));
  REQUIRE(removed.has_value());
  CHECK(*removed);
  CHECK(ns->get_info("/dst").value().fs.name == "localfs"); // backed by base again

  // An unknown name on an existing mount point is not an error, it just doesn't match
  REQUIRE(ns->mount("/dst", merge_flag::replace, true, "m", localfs{ base.root }).has_value());
  auto unknown = ns->unmount(std::string_view("nope"), std::filesystem::path("/dst"));
  REQUIRE(unknown.has_value());
  CHECK_FALSE(*unknown);

  // A missing old_path is not a mount point
  auto missing = ns->unmount(std::string_view("nope"), std::filesystem::path("/missing"));
  REQUIRE(missing.has_value());
  CHECK_FALSE(*missing);

  // A relative old_path is rejected
  auto relative = ns->unmount(std::string_view("nope"), std::filesystem::path("relative"));
  REQUIRE_FALSE(relative.has_value());
  CHECK(relative.error() == errc::not_an_abs_path);
}

TEST_CASE_METHOD(vns_fixture, "vns union shrinks one entry at a time", "[vns]") {
  REQUIRE(ns->create_directory("/u").has_value());
  tree_builder a;
  a.file("fa");
  tree_builder b;
  b.file("fb");
  tree_builder c;
  c.file("fc");

  REQUIRE(ns->mount("/u", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
  REQUIRE(ns->mount("/u", merge_flag::before, true, "b", localfs{ b.root }).has_value());
  REQUIRE(ns->mount("/u", merge_flag::after, true, "c", localfs{ c.root }).has_value());
  CHECK(ns->list("/u").value().size() == 3);

  auto remove_c = ns->unmount(std::string_view("c"), std::filesystem::path("/u"));
  REQUIRE(remove_c.has_value());
  CHECK(*remove_c);
  CHECK(ns->list("/u").value().size() == 2);
  CHECK_FALSE(ns->exists("/u/fc").value());
  CHECK(ns->exists("/u/fa").value());

  auto remove_b = ns->unmount(std::string_view("b"), std::filesystem::path("/u"));
  REQUIRE(remove_b.has_value());
  CHECK(*remove_b);
  CHECK(ns->list("/u").value().size() == 1);
  CHECK_FALSE(ns->exists("/u/fb").value());

  // Removing the last entry drops the whole mount point and restores the memfs node
  auto remove_a = ns->unmount(std::string_view("a"), std::filesystem::path("/u"));
  REQUIRE(remove_a.has_value());
  CHECK(*remove_a);
  CHECK(ns->get_info("/u").value().fs.name == "memfs");
  CHECK_FALSE(ns->exists("/u/fa").value());

  // No mount point is left, so a further unmount is a no-op
  auto remove_again = ns->unmount(std::string_view("a"), std::filesystem::path("/u"));
  REQUIRE(remove_again.has_value());
  CHECK_FALSE(*remove_again);
}

TEST_CASE_METHOD(vns_fixture, "vns unmount removes an entire mount point", "[vns]") {
  REQUIRE(ns->create_directory("/u").has_value());
  tree_builder a;
  a.file("fa");
  tree_builder b;
  b.file("fb");
  REQUIRE(ns->mount("/u", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
  REQUIRE(ns->mount("/u", merge_flag::before, true, "b", localfs{ b.root }).has_value());
  REQUIRE(ns->list("/u").value().size() == 2);

  auto removed = ns->unmount(std::filesystem::path("/u"));
  REQUIRE(removed.has_value());
  CHECK(*removed);
  CHECK(ns->get_info("/u").value().fs.name == "memfs");
  CHECK_FALSE(ns->exists("/u/fa").value());
  CHECK_FALSE(ns->exists("/u/fb").value());
}

// ---------------------------------------------------------------------------
// mount variants (name / before / after / replace onto a union)
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns named mount can be unmounted by name", "[vns]") {
  REQUIRE(ns->create_directory("/a").has_value());
  tree_builder tree;
  tree.file("f");
  REQUIRE(ns->mount("/a", merge_flag::replace, true, "m1", localfs{ tree.root }).has_value());
  CHECK(ns->get_info("/a/f").value().status == node_status::file);

  auto wrong = ns->unmount(std::string_view("m2"), std::filesystem::path("/a"));
  REQUIRE(wrong.has_value());
  CHECK_FALSE(*wrong);

  auto removed = ns->unmount(std::string_view("m1"), std::filesystem::path("/a"));
  REQUIRE(removed.has_value());
  CHECK(*removed);
  CHECK(ns->get_info("/a").value().fs.name == "memfs");
}

TEST_CASE("vns mount before/after sets union priority", "[vns]") {
  tree_builder a;
  a.file("f", "1");
  tree_builder b;
  b.file("f", "333");

  {
    auto ns = make_namespace();
    REQUIRE(ns->mount("/", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
    REQUIRE(ns->mount("/", merge_flag::before, true, "b", localfs{ b.root }).has_value());
    auto info = ns->get_info("/f");
    REQUIRE(info.has_value());
    CHECK(info->file->size == 3); // b is in front, so it wins
    CHECK(ns->list("/").value().size() == 1);
  }
  {
    auto ns = make_namespace();
    REQUIRE(ns->mount("/", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
    REQUIRE(ns->mount("/", merge_flag::after, true, "b", localfs{ b.root }).has_value());
    auto info = ns->get_info("/f");
    REQUIRE(info.has_value());
    CHECK(info->file->size == 1); // a is in front, so it wins
    CHECK(ns->list("/").value().size() == 1);
  }
}

TEST_CASE("vns mount replace shadows an existing union", "[vns]") {
  tree_builder a;
  a.file("fa");
  tree_builder b;
  b.file("fb");
  tree_builder c;
  c.file("fc");

  auto ns = make_namespace();
  REQUIRE(ns->mount("/", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
  REQUIRE(ns->mount("/", merge_flag::before, true, "b", localfs{ b.root }).has_value());
  CHECK(ns->list("/").value().size() == 2);

  REQUIRE(ns->mount("/", merge_flag::replace, true, "c", localfs{ c.root }).has_value());
  CHECK(ns->list("/").value().size() == 1);
  CHECK(ns->exists("/fc").value());
  CHECK_FALSE(ns->exists("/fa").value());
  CHECK_FALSE(ns->exists("/fb").value());
}

TEST_CASE("vns resolves components from any union entry", "[vns]") {
  tree_builder a;
  a.dir("xa");
  tree_builder b;
  b.dir("xb");

  auto ns = make_namespace();
  REQUIRE(ns->mount("/", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
  REQUIRE(ns->mount("/", merge_flag::before, true, "b", localfs{ b.root }).has_value());

  // xb only exists in b (higher priority), xa only in a (lower priority)
  CHECK(ns->status("/xb").value() == node_status::directory);
  CHECK(ns->status("/xa").value() == node_status::directory);
  CHECK(ns->list("/").value().size() == 2);
}

// ---------------------------------------------------------------------------
// path normalization
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns normalizes paths", "[vns]") {
  REQUIRE(ns->create_directory("/a").has_value());
  REQUIRE(ns->create_directory("/a/b").has_value());

  CHECK(ns->status("/a/b/").value() == node_status::directory);
  CHECK(ns->status("/a/./b").value() == node_status::directory);
  CHECK(ns->status("/a/b/..").value() == node_status::directory);
  CHECK(ns->status("/a//b").value() == node_status::directory);
  CHECK(ns->list("/a/").value().size() == 1);

  // A trailing slash is removed on creation as well
  REQUIRE(ns->create_directory("/c/d", true).has_value());
  CHECK(ns->status("/c/d/").value() == node_status::directory);

  // The root has no name and can't be created
  auto root_dir = ns->create_directory("/");
  REQUIRE_FALSE(root_dir.has_value());
  CHECK(root_dir.error() == errc::invalid_name);
  auto root_file = ns->create_file("/", placeholder_file_create_spec{});
  REQUIRE_FALSE(root_file.has_value());
  CHECK(root_file.error() == errc::invalid_name);
}

// ---------------------------------------------------------------------------
// list / status / exists / get_info edge cases
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns query edge cases", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  REQUIRE(ns->create_file("/f", placeholder_file_create_spec{}).has_value());

  auto list_file = ns->list("/f");
  REQUIRE_FALSE(list_file.has_value());
  CHECK(list_file.error() == errc::not_a_dir);

  auto list_missing = ns->list("/missing");
  REQUIRE_FALSE(list_missing.has_value());
  CHECK(list_missing.error() == errc::path_not_found);

  CHECK(ns->status("/f").value() == node_status::file);
  CHECK(ns->exists("/f", node_type::file).value());
  CHECK_FALSE(ns->exists("/f", node_type::directory).value());
  CHECK(ns->exists("/d", node_type::directory).value());

  auto list_rel = ns->list("relative");
  REQUIRE_FALSE(list_rel.has_value());
  CHECK(list_rel.error() == errc::not_an_abs_path);
  auto exists_rel = ns->exists("relative");
  REQUIRE_FALSE(exists_rel.has_value());
  CHECK(exists_rel.error() == errc::not_an_abs_path);
  auto exists_nt_rel = ns->exists("relative", node_type::file);
  REQUIRE_FALSE(exists_nt_rel.has_value());
  CHECK(exists_nt_rel.error() == errc::not_an_abs_path);
}

TEST_CASE_METHOD(vns_fixture, "vns get_info reports the fs-relative path", "[vns]") {
  tree_builder root_tree;
  root_tree.dir("a");
  tree_builder sub_tree;
  sub_tree.file("hello");

  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ root_tree.root }).has_value());
  REQUIRE(ns->mount("/a", merge_flag::replace, true, localfs{ sub_tree.root }).has_value());

  auto info = ns->get_info("/a/hello");
  REQUIRE(info.has_value());
  CHECK(info->status == node_status::file);
  CHECK(info->name == "hello");
  CHECK(info->fs.name == "localfs");
  CHECK(info->fs.path == std::filesystem::path("/hello"));
}

// ---------------------------------------------------------------------------
// create / remove
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns create/remove error cases", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  REQUIRE(ns->create_file("/f", placeholder_file_create_spec{}).has_value());

  auto existing = ns->create_directory("/d");
  REQUIRE_FALSE(existing.has_value());
  CHECK(existing.error() == std::errc::file_exists);

  auto parent_file = ns->create_directory("/f/child");
  REQUIRE_FALSE(parent_file.has_value());
  CHECK(parent_file.error() == errc::not_a_dir);

  auto file_existing = ns->create_file("/f", placeholder_file_create_spec{});
  REQUIRE_FALSE(file_existing.has_value());
  CHECK(file_existing.error() == std::errc::file_exists);

  CHECK(ns->remove("/f").has_value());
  CHECK_FALSE(ns->exists("/f").value());
}

TEST_CASE_METHOD(vns_fixture, "vns create and remove under a mount", "[vns]") {
  tree_builder tree;
  tree.dir("a");
  REQUIRE(ns->mount("/", merge_flag::replace, true, localfs{ tree.root }).has_value());

  // localfs has no placeholder concept: a placeholder file is a regular empty file there
  REQUIRE(ns->create_file("/a/p", placeholder_file_create_spec{}).has_value());
  CHECK(ns->get_info("/a/p").value().status == node_status::file);
  CHECK(ns->get_info("/a/p").value().file->size == 0);

  regular_file_create_spec regular_spec;
  regular_spec.initial_size = 4;
  REQUIRE(ns->create_file("/a/r", regular_spec).has_value());
  CHECK(ns->get_info("/a/r").value().file->size == 4);

  scalar_file_create_spec scalar_spec;
  scalar_spec.init_value = std::any(1);
  auto scalar = ns->create_file("/a/s", scalar_spec);
  REQUIRE_FALSE(scalar.has_value());
  CHECK(scalar.error() == errc::unsupported);

  CHECK(ns->remove("/a/r").has_value());
  CHECK_FALSE(ns->exists("/a/r").value());

  // A read-only mount rejects create and remove
  tree_builder ro;
  ro.dir("a");
  auto ro_ns = make_namespace();
  REQUIRE(ro_ns->mount("/", merge_flag::replace, false, localfs{ ro.root }).has_value());
  auto rmdir = ro_ns->remove("/a");
  REQUIRE_FALSE(rmdir.has_value());
  CHECK(rmdir.error() == errc::unsupported);
  auto mkfile = ro_ns->create_file("/a/x", placeholder_file_create_spec{});
  REQUIRE_FALSE(mkfile.has_value());
  CHECK(mkfile.error() == errc::unsupported);
}

TEST_CASE_METHOD(vns_fixture, "vns remove rejects a mount point", "[vns]") {
  REQUIRE(ns->create_directory("/a").has_value());
  tree_builder tree;
  tree.dir("sub");
  REQUIRE(ns->mount("/a", merge_flag::replace, true, "a", localfs{ tree.root }).has_value());

  auto removed = ns->remove("/a");
  REQUIRE_FALSE(removed.has_value());
  CHECK(removed.error() == errc::unsupported);

  // The mount and the underlying file system are untouched
  CHECK(ns->get_info("/a").value().fs.name == "localfs");
  CHECK(ns->exists("/a/sub").value());
}

TEST_CASE_METHOD(vns_fixture, "vns freeze blocks create and remove", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  ns->freeze();

  auto mkdir = ns->create_directory("/x");
  REQUIRE_FALSE(mkdir.has_value());
  CHECK(mkdir.error() == errc::frozen);

  auto mkfile = ns->create_file("/f", placeholder_file_create_spec{});
  REQUIRE_FALSE(mkfile.has_value());
  CHECK(mkfile.error() == errc::frozen);

  auto rm = ns->remove("/d");
  REQUIRE_FALSE(rm.has_value());
  CHECK(rm.error() == errc::frozen);
}

// ---------------------------------------------------------------------------
// open_file / read-write helpers
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns open_file variants and control", "[vns]") {
  REQUIRE(ns->create_directory("/d").has_value());
  REQUIRE(ns->create_file("/d/r", regular_file_create_spec{}).has_value());
  regular_file_create_spec mapped_spec;
  mapped_spec.initial_size = 8;
  REQUIRE(ns->create_file("/d/m", mapped_spec).has_value());
  scalar_file_create_spec scalar_spec;
  scalar_spec.init_value = std::any(7);
  REQUIRE(ns->create_file("/d/s", scalar_spec).has_value());

  CHECK(ns->open_file("/d/r", seq_stream_file_open_options{}).has_value());
  CHECK(ns->open_file("/d/m", mapped_file_open_options{}).has_value());
  CHECK(ns->open_file("/d/s", scalar_file_open_options{}).has_value());

  // memfs doesn't support control files
  auto control = ns->open_file("/d/r", control_file_open_options{});
  REQUIRE_FALSE(control.has_value());
  CHECK(control.error() == errc::unsupported);
  auto executed = ns->execute("/d/r", "cmd", control_file_open_options{});
  REQUIRE_FALSE(executed.has_value());
  CHECK(executed.error() == errc::unsupported);

  std::array<std::byte, 4> buffer{};
  std::array<mutable_buffer, 1> read_buffers{ mutable_buffer(buffer) };
  CHECK_FALSE(ns->read_all("/missing", read_buffers, {}).has_value());
  CHECK_FALSE(ns->read_all("/d", read_buffers, {}).has_value());
}

TEST_CASE("vns write dispatch across a union", "[vns]") {
  auto ns = make_namespace();
  REQUIRE(ns->create_directory("/m").has_value());
  REQUIRE(ns->mount("/m", merge_flag::replace, true, "m", memfs{}).has_value());
  REQUIRE(ns->create_file("/m/memf", regular_file_create_spec{}).has_value());

  tree_builder ro;
  ro.file("rof", "ro"); // 2 bytes
  REQUIRE(ns->mount("/m", merge_flag::before, false, "ro", localfs{ ro.root }).has_value());

  // A name only present in the writable memfs entry resolves through the union
  CHECK(ns->get_info("/m/memf").value().status == node_status::file);
  CHECK(ns->get_info("/m/rof").value().status == node_status::file);

  // /m/rof is served by the read-only (higher priority) entry, so writing to it fails
  stream_file_open_options write_options;
  write_options.mode = file_open_mode::write;
  auto ro_write = ns->open_file("/m/rof", write_options);
  REQUIRE_FALSE(ro_write.has_value());
  CHECK(ro_write.error() == errc::unsupported);

  // /m/memf is only in the writable entry, so writing it succeeds
  auto mem_write = ns->open_file("/m/memf", write_options);
  REQUIRE(mem_write.has_value());

  // Creating a new child picks the first writable entry of the union
  REQUIRE(ns->create_directory("/m/newdir").has_value());
  CHECK(ns->status("/m/newdir").value() == node_status::directory);
}

// ---------------------------------------------------------------------------
// shadow over unions / dynamic inheritance
// ---------------------------------------------------------------------------

TEST_CASE_METHOD(vns_fixture, "vns shadow copies an inherited union on mount", "[vns]") {
  tree_builder a;
  a.file("fa");
  tree_builder b;
  b.file("fb");
  tree_builder c;
  c.file("fc");
  REQUIRE(ns->mount("/", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
  REQUIRE(ns->mount("/", merge_flag::before, true, "b", localfs{ b.root }).has_value());

  auto shadow = *ns->shadow();
  // Adding to an inherited multi-entry mount point must copy-on-write
  REQUIRE(shadow->mount("/", merge_flag::before, true, "c", localfs{ c.root }).has_value());
  CHECK(shadow->get_info("/fc").value().status == node_status::file);
  CHECK(shadow->get_info("/fa").value().status == node_status::file);
  CHECK(shadow->list("/").value().size() == 3);

  // The parent is untouched
  CHECK(ns->get_info("/fc").value().status == node_status::not_found);
  CHECK(ns->get_info("/fa").value().status == node_status::file);
  CHECK(ns->list("/").value().size() == 2);
}

TEST_CASE_METHOD(vns_fixture, "vns shadow removes one entry from an inherited union", "[vns]") {
  tree_builder a;
  a.file("fa");
  tree_builder b;
  b.file("fb");
  REQUIRE(ns->mount("/", merge_flag::replace, true, "a", localfs{ a.root }).has_value());
  REQUIRE(ns->mount("/", merge_flag::before, true, "b", localfs{ b.root }).has_value());

  auto shadow = *ns->shadow();
  auto removed = shadow->unmount(std::string_view("b"), std::filesystem::path("/"));
  REQUIRE(removed.has_value());
  CHECK(*removed);

  // The shadow no longer sees fb but still sees fa; the parent keeps both
  CHECK(shadow->get_info("/fb").value().status == node_status::not_found);
  CHECK(shadow->get_info("/fa").value().status == node_status::file);
  CHECK(ns->get_info("/fb").value().status == node_status::file);
  CHECK(ns->list("/").value().size() == 2);
}

TEST_CASE_METHOD(vns_fixture, "vns shadow sees later parent mounts", "[vns]") {
  REQUIRE(ns->create_directory("/m").has_value());
  auto shadow = *ns->shadow();
  CHECK(shadow->get_info("/m").value().fs.name == "memfs");

  tree_builder tree;
  tree.file("inside");
  REQUIRE(ns->mount("/m", merge_flag::replace, true, "m", localfs{ tree.root }).has_value());

  CHECK(shadow->get_info("/m").value().fs.name == "localfs");
  CHECK(shadow->get_info("/m/inside").value().status == node_status::file);
}
