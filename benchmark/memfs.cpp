/**
 * @file memfs.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief memfs benchmarks
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "vfs9/fs/mem/fs.hpp"

namespace {

using memfs_impl = vfs9::fs::mem::fs_impl;

constexpr std::size_t kFileSize = 4096;

memfs_impl make_fs() {
  memfs_impl fs;
  fs.init(vfs9::memfs{});
  return fs;
}

void build_files(memfs_impl& fs, std::size_t n) {
  fs.create_directory("/data");
  for (std::size_t i = 0; i < n; ++i) {
    fs.create_file("/data/f" + std::to_string(i), vfs9::regular_file_create_spec{});
  }
}

std::vector<std::filesystem::path> file_paths(std::size_t n) {
  std::vector<std::filesystem::path> paths;
  paths.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    paths.emplace_back("/data/f" + std::to_string(i));
  }
  return paths;
}

// ------ create / remove

void BM_MemfsCreateDirectory(benchmark::State& state) {
  auto fs = make_fs();
  std::size_t i = 0;
  for (auto _ : state) {
    const std::string name = "/d" + std::to_string(i++);
    auto result = fs.create_directory(name);
    benchmark::DoNotOptimize(result);
    state.PauseTiming();
    fs.remove(name);
    state.ResumeTiming();
  }
}

void BM_MemfsCreateFilePlaceholder(benchmark::State& state) {
  auto fs = make_fs();
  std::size_t i = 0;
  for (auto _ : state) {
    const std::string name = "/p" + std::to_string(i++);
    auto result = fs.create_file(name, vfs9::placeholder_file_create_spec{});
    benchmark::DoNotOptimize(result);
    state.PauseTiming();
    fs.remove(name);
    state.ResumeTiming();
  }
}

void BM_MemfsCreateFileRegular(benchmark::State& state) {
  auto fs = make_fs();
  vfs9::regular_file_create_spec spec;
  spec.initial_size = kFileSize;
  std::size_t i = 0;
  for (auto _ : state) {
    const std::string name = "/r" + std::to_string(i++);
    auto result = fs.create_file(name, spec);
    benchmark::DoNotOptimize(result);
    state.PauseTiming();
    fs.remove(name);
    state.ResumeTiming();
  }
}

void BM_MemfsRemove(benchmark::State& state) {
  auto fs = make_fs();
  std::size_t i = 0;
  for (auto _ : state) {
    const std::string name = "/r" + std::to_string(i++);
    state.PauseTiming();
    fs.create_file(name, vfs9::regular_file_create_spec{});
    state.ResumeTiming();
    auto result = fs.remove(name);
    benchmark::DoNotOptimize(result);
  }
}

// ------ queries

void BM_MemfsExistsHit(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto fs = make_fs();
  build_files(fs, n);
  const auto paths = file_paths(n);
  for (auto _ : state) {
    for (const auto& path : paths) {
      auto result = fs.exists(path);
      benchmark::DoNotOptimize(result);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

void BM_MemfsExistsMiss(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto fs = make_fs();
  build_files(fs, n);
  for (auto _ : state) {
    auto result = fs.exists("/data/missing");
    benchmark::DoNotOptimize(result);
  }
}

void BM_MemfsStatus(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto fs = make_fs();
  build_files(fs, n);
  const auto paths = file_paths(n);
  for (auto _ : state) {
    for (const auto& path : paths) {
      auto status = fs.status(path);
      benchmark::DoNotOptimize(status);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

void BM_MemfsGetInfo(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto fs = make_fs();
  build_files(fs, n);
  const auto paths = file_paths(n);
  for (auto _ : state) {
    for (const auto& path : paths) {
      auto info = fs.get_info(path);
      benchmark::DoNotOptimize(info);
    }
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

void BM_MemfsList(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto fs = make_fs();
  build_files(fs, n);
  std::vector<vfs9::node_entry> entries;
  for (auto _ : state) {
    auto result = fs.list("/data", entries);
    benchmark::DoNotOptimize(result);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

// ------ file i/o

void BM_MemfsOpenFile(benchmark::State& state) {
  const auto n = static_cast<std::size_t>(state.range(0));
  auto fs = make_fs();
  build_files(fs, n);
  const std::filesystem::path target = "/data/f0";
  for (auto _ : state) {
    auto file = fs.open_file(target, vfs9::stream_file_open_options{});
    benchmark::DoNotOptimize(file);
  }
}

void BM_MemfsStreamRead(benchmark::State& state) {
  auto fs = make_fs();
  vfs9::regular_file_create_spec spec;
  spec.initial_size = kFileSize;
  fs.create_file("/big", spec);
  auto file = fs.open_file("/big", vfs9::stream_file_open_options{});
  auto& stream = *file.value();

  std::array<std::byte, kFileSize> buffer{};
  std::array<vfs9::mutable_buffer, 1> buffers{ vfs9::mutable_buffer(buffer) };
  for (auto _ : state) {
    auto result = stream.read(0, buffers);
    benchmark::DoNotOptimize(result);
  }
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(kFileSize));
}

void BM_MemfsStreamWrite(benchmark::State& state) {
  auto fs = make_fs();
  vfs9::regular_file_create_spec spec;
  spec.initial_size = kFileSize;
  fs.create_file("/big", spec);
  vfs9::stream_file_open_options options;
  options.mode = vfs9::file_open_mode::write;
  auto file = fs.open_file("/big", options);
  auto& stream = *file.value();

  std::array<std::byte, kFileSize> buffer{};
  std::array<vfs9::const_buffer, 1> buffers{ vfs9::const_buffer(buffer) };
  for (auto _ : state) {
    auto result = stream.write(0, buffers);
    benchmark::DoNotOptimize(result);
  }
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(kFileSize));
}

void BM_MemfsSeqStreamRead(benchmark::State& state) {
  auto fs = make_fs();
  vfs9::regular_file_create_spec spec;
  spec.initial_size = kFileSize;
  fs.create_file("/big", spec);
  auto file = fs.open_file("/big", vfs9::seq_stream_file_open_options{});
  auto& stream = *file.value();

  std::array<std::byte, kFileSize> buffer{};
  std::array<vfs9::mutable_buffer, 1> buffers{ vfs9::mutable_buffer(buffer) };
  for (auto _ : state) {
    state.PauseTiming();
    stream.seek_read(0, vfs9::seq_stream_file::seek_whence::begin);
    state.ResumeTiming();
    auto result = stream.read(buffers);
    benchmark::DoNotOptimize(result);
  }
  state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(kFileSize));
}

} // namespace

BENCHMARK(BM_MemfsCreateDirectory);
BENCHMARK(BM_MemfsCreateFilePlaceholder);
BENCHMARK(BM_MemfsCreateFileRegular);
BENCHMARK(BM_MemfsRemove);
BENCHMARK(BM_MemfsExistsHit)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_MemfsExistsMiss)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_MemfsStatus)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_MemfsGetInfo)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_MemfsList)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_MemfsOpenFile)->Arg(10)->Arg(200)->Arg(5000);
BENCHMARK(BM_MemfsStreamRead);
BENCHMARK(BM_MemfsStreamWrite);
BENCHMARK(BM_MemfsSeqStreamRead);

BENCHMARK_MAIN();
