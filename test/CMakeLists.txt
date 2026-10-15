cmake_minimum_required(VERSION 4.0.0)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.2
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(Catch2)

find_package(minizip-ng CONFIG REQUIRED)

enable_testing()

add_executable(vfs9_test_node_tree node_tree.cpp)
target_include_directories(vfs9_test_node_tree PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vfs9_test_node_tree PRIVATE Catch2::Catch2WithMain)

add_executable(vfs9_test_memfs memfs.cpp)
target_link_libraries(vfs9_test_memfs PRIVATE vfs9::static_lib Catch2::Catch2WithMain)

add_executable(vfs9_test_localfs localfs.cpp)
target_link_libraries(vfs9_test_localfs PRIVATE vfs9::static_lib Catch2::Catch2WithMain)

add_executable(vfs9_test_vns vns.cpp)
target_link_libraries(vfs9_test_vns PRIVATE vfs9::static_lib Catch2::Catch2WithMain)

add_executable(vfs9_test_pakfs pak.cpp)
target_link_libraries(vfs9_test_pakfs PRIVATE vfs9::static_lib Catch2::Catch2WithMain)

add_executable(vfs9_test_zipfs zipfs.cpp)
# The test uses the minizip-ng writer directly; only its include dirs are needed (the library itself is
# already pulled in transitively through vfs9::static_lib).
get_target_property(MINIZIP_INCLUDE_DIRS MINIZIP::minizip-ng INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(vfs9_test_zipfs PRIVATE ${MINIZIP_INCLUDE_DIRS})
target_link_libraries(vfs9_test_zipfs PRIVATE vfs9::static_lib Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(vfs9_test_node_tree)
catch_discover_tests(vfs9_test_memfs)
catch_discover_tests(vfs9_test_localfs)
catch_discover_tests(vfs9_test_vns)
catch_discover_tests(vfs9_test_pakfs)
catch_discover_tests(vfs9_test_zipfs)
