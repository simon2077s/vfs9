cmake_minimum_required(VERSION 4.0.0)

set(BENCHMARK_ENABLE_TESTING OFF)

include(FetchContent)

FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark
  GIT_TAG v1.9.5
)
FetchContent_MakeAvailable(benchmark)

# node_tree is header-only, so only the include directory is needed.
add_executable(vfs9_benchmark_node_tree node_tree.cpp)
target_include_directories(vfs9_benchmark_node_tree PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vfs9_benchmark_node_tree PRIVATE benchmark::benchmark_main)

add_executable(vfs9_benchmark_memfs memfs.cpp)
target_link_libraries(vfs9_benchmark_memfs PRIVATE vfs9::static_lib benchmark::benchmark_main)
