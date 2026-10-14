/**
 * @file node_tree.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Node tree unit test cases
 * @version 0.1
 * @date 2026-10-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "vfs9/detail/node_tree.hpp"

#include <filesystem>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

struct base {
  int x = 0;
};

using tree_t = vfs9::detail::node_tree<base>;
using node_t = tree_t::node_type;
using map_t = vfs9::detail::node_tree_node_map<base>;

node_t make_dir(std::string_view name) {
  return std::make_shared<vfs9::detail::node_tree_node<base>>(vfs9::node_type::directory, std::string(name));
}

node_t make_file(std::string_view name) {
  return std::make_shared<vfs9::detail::node_tree_node<base>>(vfs9::node_type::file, std::string(name));
}

node_t lookup(const tree_t& tree, const std::filesystem::path& path) {
  auto begin = path.begin();
  auto end = path.end();
  return tree.find(begin, end);
}

} // namespace

TEST_CASE("node_tree_node_map is empty by default", "[node_tree]") {
  map_t map;
  CHECK(map.empty());
  CHECK(map.size() == 0);
  CHECK_FALSE(map.contains("missing"));
  CHECK(map.find("missing") == nullptr);
}

TEST_CASE("node_tree_node_map set and overwrite semantics", "[node_tree]") {
  map_t map;
  auto first = make_file("first");
  auto second = make_file("second");

  CHECK(map.set("a", first));
  CHECK(map.size() == 1);
  CHECK(map.contains("a"));
  CHECK(map.find("a") == first);

  CHECK_FALSE(map.set("a", second, false));
  CHECK(map.find("a") == first);
  CHECK(map.set("a", second, true));
  CHECK(map.find("a") == second);
  CHECK(map.size() == 1);
}

TEST_CASE("node_tree_node_map erase and clear", "[node_tree]") {
  map_t map;
  map.set("a", make_file("a"));
  map.set("b", make_file("b"));
  REQUIRE(map.size() == 2);

  CHECK_FALSE(map.erase("missing"));
  CHECK(map.erase("a"));
  CHECK(map.size() == 1);
  CHECK_FALSE(map.contains("a"));

  map.clear();
  CHECK(map.empty());
  CHECK(map.size() == 0);
}

TEST_CASE("node_tree_node_map for_each visits every node", "[node_tree]") {
  map_t map;
  map.set("a", make_file("a"));
  map.set("b", make_file("b"));
  map.set("c", make_file("c"));

  size_t visits = 0;
  map.for_each([&](std::string_view name, const node_t& p_node) {
    CHECK_FALSE(name.empty());
    CHECK(p_node != nullptr);
    ++visits;
  });
  CHECK(visits == 3);
}

TEST_CASE("node_tree_node_map promotes and demotes around the SOO threshold", "[node_tree]") {
  constexpr size_t threshold = 32;
  map_t map;
  for (size_t i = 0; i < threshold + 8; ++i) {
    CHECK(map.set("k" + std::to_string(i), make_file(std::to_string(i))));
  }
  CHECK(map.size() == threshold + 8);
  CHECK(map.contains("k39"));

  CHECK_FALSE(map.set("k0", make_file("dup"), false));
  CHECK(map.set("k0", make_file("dup"), true));

  for (size_t i = 0; i < threshold + 8; ++i) {
    CHECK(map.erase("k" + std::to_string(i)));
  }
  CHECK(map.empty());
  CHECK(map.size() == 0);

  CHECK(map.set("solo", make_file("solo")));
  CHECK(map.size() == 1);
  CHECK(map.contains("solo"));
}

TEST_CASE("node_tree_node forwards extra arguments to the base", "[node_tree]") {
  auto p_node = std::make_shared<vfs9::detail::node_tree_node<base>>(vfs9::node_type::file, "f", base{ 42 });
  CHECK(p_node->type() == vfs9::node_type::file);
  CHECK(p_node->name() == "f");
  CHECK(p_node->x == 42);
  CHECK(p_node->children().empty());
}

TEST_CASE("node_tree find and contains traverse absolute paths", "[node_tree]") {
  tree_t tree(make_dir("/"));
  auto p_node = make_file("c");
  const std::filesystem::path target("/a/b/c");
  {
    auto begin = target.begin();
    auto end = target.end();
    REQUIRE(tree.recursive_set(begin, end, make_dir, p_node, true));
  }

  const std::filesystem::path root_path("/");
  const std::filesystem::path ab("/a/b");
  const std::filesystem::path abcd("/a/b/c/d");
  const std::filesystem::path missing("/missing");

  CHECK(lookup(tree, root_path) == tree.root());
  CHECK(lookup(tree, ab) != nullptr);
  CHECK(lookup(tree, target) == p_node);
  CHECK(lookup(tree, abcd) == nullptr);
  CHECK(lookup(tree, missing) == nullptr);

  {
    auto begin = target.begin();
    auto end = target.end();
    CHECK(tree.contains(begin, end) == end);
  }
  {
    const std::filesystem::path partial("/a/x");
    auto begin = partial.begin();
    auto end = partial.end();
    auto after = tree.contains(begin, end);
    CHECK(after != end);
    CHECK(after->string() == "x");
  }
}

TEST_CASE("node_tree recursive_set creates missing directories", "[node_tree]") {
  tree_t tree(make_dir("/"));

  size_t created = 0;
  auto counting_make_dir = [&](std::string_view name) -> node_t {
    ++created;
    return make_dir(name);
  };

  const std::filesystem::path first("/x/y/z");
  {
    auto begin = first.begin();
    auto end = first.end();
    REQUIRE(tree.recursive_set(begin, end, counting_make_dir, make_file("z"), true));
  }
  CHECK(created == 2);
  CHECK(lookup(tree, "/x")->type() == vfs9::node_type::directory);
  CHECK(lookup(tree, "/x/y")->type() == vfs9::node_type::directory);
  CHECK(lookup(tree, first)->type() == vfs9::node_type::file);

  created = 0;
  const std::filesystem::path second("/x/y/w");
  {
    auto begin = second.begin();
    auto end = second.end();
    REQUIRE(tree.recursive_set(begin, end, counting_make_dir, make_file("w"), true));
  }
  CHECK(created == 0);
}

TEST_CASE("node_tree set requires an existing parent and honors overwrite", "[node_tree]") {
  tree_t tree(make_dir("/"));
  const std::filesystem::path a("/a");
  {
    auto begin = a.begin();
    auto end = a.end();
    REQUIRE(tree.set(begin, end, make_dir("a"), false));
  }

  auto first = make_file("f");
  auto second = make_file("f");
  const std::filesystem::path af("/a/f");
  {
    auto begin = af.begin();
    auto end = af.end();
    CHECK(tree.set(begin, end, first, false));
    CHECK_FALSE(tree.set(begin, end, second, false));
    CHECK(lookup(tree, af) == first);
    CHECK(tree.set(begin, end, second, true));
    CHECK(lookup(tree, af) == second);
  }
  {
    const std::filesystem::path missing("/missing/f");
    auto begin = missing.begin();
    auto end = missing.end();
    CHECK_FALSE(tree.set(begin, end, make_file("f"), false));
  }
}

TEST_CASE("node_tree erase guards non-empty directories unless recursive", "[node_tree]") {
  tree_t tree(make_dir("/"));
  const std::filesystem::path abc("/a/b/c");
  {
    auto begin = abc.begin();
    auto end = abc.end();
    REQUIRE(tree.recursive_set(begin, end, make_dir, make_file("c"), true));
  }

  {
    auto begin = abc.begin();
    auto end = abc.end();
    CHECK(tree.erase(begin, end, false));
  }
  CHECK(lookup(tree, abc) == nullptr);

  const std::filesystem::path a("/a");
  {
    auto begin = a.begin();
    auto end = a.end();
    CHECK_FALSE(tree.erase(begin, end, false));
    CHECK(tree.erase(begin, end, true));
  }
  CHECK(lookup(tree, a) == nullptr);

  {
    const std::filesystem::path nope("/nope");
    auto begin = nope.begin();
    auto end = nope.end();
    CHECK_FALSE(tree.erase(begin, end, false));
  }
}

TEST_CASE("node_tree clear removes all nodes but keeps the root", "[node_tree]") {
  tree_t tree(make_dir("/"));
  const std::filesystem::path ab("/a/b");
  {
    auto begin = ab.begin();
    auto end = ab.end();
    REQUIRE(tree.recursive_set(begin, end, make_dir, make_file("b"), true));
  }

  tree.clear();
  CHECK(tree.root() != nullptr);
  CHECK(tree.root()->children().empty());
  CHECK(lookup(tree, "/a") == nullptr);
}
