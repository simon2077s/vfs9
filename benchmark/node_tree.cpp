/**
 * @file node_tree.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief node_tree benchmarks
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <benchmark/benchmark.h>

#include "vfs9/detail/node_tree.hpp"

namespace {

/**
 * @brief Minimal node base used by the benchmarks
 *
 */
struct bench_node_base {};

using tree_type = vfs9::detail::node_tree<bench_node_base>;
using node_ptr = tree_type::node_type;
using node_type = vfs9::node_type;

node_ptr make_node(node_type type, std::string name) {
  return std::make_shared<vfs9::detail::node_tree_node<bench_node_base>>(type, std::move(name));
}

/**
 * @brief A pre-built tree plus the sample paths used by the benchmarks
 *
 */
struct built_tree {
  tree_type tree;
  std::vector<std::filesystem::path> paths; // All existing node paths
  std::filesystem::path big_dir;            // The directory with the most children
  std::vector<std::string> big_dir_names;   // Names of the children in `big_dir`
};

/**
 * @brief Build a tree with exactly `n` nodes (root included)
 *
 *  For n >= 200 the directories hold more than 32 children, so `node_tree_node_map` promotes from the
 *  small-object array representation to the hash map; for n == 10 it stays in the array.
 *
 * @param n Total number of nodes
 * @return built_tree
 */
built_tree build_tree(std::size_t n) {
  built_tree result;
  result.tree = tree_type(make_node(node_type::directory, "/"));
  const auto root = result.tree.root();

  const std::size_t sub_count = n >= 200 ? 4 : 2;
  const std::size_t file_count = n > 1 + sub_count ? n - 1 - sub_count : 0;

  std::vector<std::string> dir_names;
  dir_names.reserve(sub_count);
  for (std::size_t i = 0; i < sub_count; ++i) {
    std::string name = "d" + std::to_string(i);
    root->children().set(name, make_node(node_type::directory, name), false);
    dir_names.push_back(std::move(name));
    result.paths.emplace_back("/" + dir_names.back());
  }

  for (std::size_t i = 0; i < file_count; ++i) {
    const std::string& dir_name = dir_names[i % sub_count];
    std::string file_name = "f" + std::to_string(i);
    root->children().find(dir_name)->children().set(file_name, make_node(node_type::file, file_name), false);
    result.paths.emplace_back("/" + dir_name + "/" + file_name);
  }

  std::size_t best = 0;
  for (std::size_t i = 1; i < dir_names.size(); ++i) {
    if (root->children().find(dir_names[i])->children().size() >
        root->children().find(dir_names[best])->children().size()) {
      best = i;
    }
  }
  result.big_dir = "/" + dir_names[best];
  root->children().find(dir_names[best])->children().for_each(
      [&result](std::string_view name, const node_ptr&) { result.big_dir_names.emplace_back(name); });
  return result;
}

void BM_NodeTreeFind(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  for (auto _ : state) {
    for (const auto& path : data.paths) {
      auto node = data.tree.find(path);
      benchmark::DoNotOptimize(node);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(data.paths.size()));
}

void BM_NodeTreeFindMiss(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  const std::filesystem::path missing = data.big_dir / "does_not_exist";
  for (auto _ : state) {
    auto node = data.tree.find(missing);
    benchmark::DoNotOptimize(node);
  }
}

void BM_NodeTreeContains(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  for (auto _ : state) {
    for (const auto& path : data.paths) {
      auto begin = path.begin();
      auto end = path.end();
      auto it = data.tree.contains(begin, end);
      benchmark::DoNotOptimize(it);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(data.paths.size()));
}

void BM_NodeTreeChildrenFind(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  auto dir = data.tree.find(data.big_dir);
  for (auto _ : state) {
    for (const auto& name : data.big_dir_names) {
      auto child = dir->children().find(name);
      benchmark::DoNotOptimize(child);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(data.big_dir_names.size()));
}

void BM_NodeTreeForEach(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  auto dir = data.tree.find(data.big_dir);
  for (auto _ : state) {
    std::size_t count = 0;
    dir->children().for_each([&count](std::string_view, const node_ptr&) { ++count; });
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(data.big_dir_names.size()));
}

void BM_NodeTreeSet(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  const std::filesystem::path target = data.paths.back();
  for (auto _ : state) {
    auto begin = target.begin();
    auto end = target.end();
    data.tree.set(begin, end, make_node(node_type::file, target.filename().string()), true);
  }
}

void BM_NodeTreeRecursiveSet(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  const std::filesystem::path target = data.big_dir / "new_sub" / "new_leaf";
  auto new_dir = [](std::string_view name) -> node_ptr { return make_node(node_type::directory, std::string(name)); };
  for (auto _ : state) {
    auto begin = target.begin();
    auto end = target.end();
    data.tree.recursive_set(begin, end, new_dir, make_node(node_type::file, "new_leaf"), false);
    state.PauseTiming();
    auto erase_begin = target.begin();
    auto erase_end = target.end();
    data.tree.erase(erase_begin, erase_end, true);
    state.ResumeTiming();
  }
}

void BM_NodeTreeErase(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto data = build_tree(n);
  const std::filesystem::path target = data.big_dir / "leaf_to_erase";
  for (auto _ : state) {
    state.PauseTiming();
    auto begin = target.begin();
    auto end = target.end();
    data.tree.set(begin, end, make_node(node_type::file, "leaf_to_erase"), false);
    state.ResumeTiming();

    auto erase_begin = target.begin();
    auto erase_end = target.end();
    data.tree.erase(erase_begin, erase_end, false);
  }
}

void BM_NodeTreeClear(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  for (auto _ : state) {
    state.PauseTiming();
    auto data = build_tree(n);
    state.ResumeTiming();
    data.tree.clear();
    benchmark::DoNotOptimize(data.tree);
  }
}

} // namespace

BENCHMARK(BM_NodeTreeFind)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeFindMiss)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeContains)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeChildrenFind)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeForEach)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeSet)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeRecursiveSet)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeErase)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_NodeTreeClear)->Arg(10)->Arg(200)->Arg(5000);

BENCHMARK_MAIN();
