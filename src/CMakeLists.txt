cmake_minimum_required(VERSION 4.0.0)

include(FetchContent)

# cpp-linenoise: line editing (cursor movement, up/down history navigation)
FetchContent_Declare(
  cpp-linenoise
  GIT_REPOSITORY https://github.com/yhirose/cpp-linenoise.git
  GIT_TAG b0e8b88f53be0de26252180b1b959c6dd67ec4a9
)
FetchContent_MakeAvailable(cpp-linenoise)

find_package(Boost CONFIG REQUIRED program_options json)
find_package(Threads REQUIRED)
find_package(llfio CONFIG REQUIRED)

add_library(vfs9_static_lib STATIC)
add_library(vfs9::static_lib ALIAS vfs9_static_lib)

target_include_directories(vfs9_static_lib PUBLIC ../src)
target_link_libraries(vfs9_static_lib PUBLIC Boost::boost)
target_link_libraries(vfs9_static_lib PUBLIC llfio::sl)

# Header files
set(
  HEADER_FILES
  vfs9.hpp
  vfs9/buffer.hpp
  vfs9/error.hpp
  vfs9/file.hpp
  vfs9/fs.hpp
  vfs9/tracer.hpp
  vfs9/vns.hpp
  vfs9/memfs/memfs.hpp
  vfs9/memfs/node.hpp
  vfs9/memfs/base_file_handle.hpp
  vfs9/memfs/stream_file_handle.hpp
  vfs9/memfs/seq_stream_file_handle.hpp
  vfs9/memfs/mapped_file_handle.hpp
  vfs9/memfs/scalar_file_handle.hpp
  vfs9/localfs/localfs.hpp
  vfs9/localfs/base_file_handle.hpp
  vfs9/localfs/stream_file_handle.hpp
  vfs9/localfs/seq_stream_file_handle.hpp
  vfs9/localfs/mapped_file_handle.hpp
  vfs9/detail/file_handle.hpp
  vfs9/detail/fs.hpp
  vfs9/detail/lock.hpp
  vfs9/detail/mount_entry.hpp
  vfs9/detail/mount_point.hpp
  vfs9/detail/mount_table.hpp
  vfs9/detail/node_id.hpp
  vfs9/detail/node_ref.hpp
  vfs9/detail/node_tree.hpp
  vfs9/detail/type_traits.hpp
  vfs9/detail/utility.hpp
)

# Source files
set(
  SOURCE_FILES
  vfs9/error.cpp
  vfs9/file.cpp
  vfs9/vns.cpp
  vfs9/memfs/memfs.cpp
  vfs9/memfs/node.cpp
  vfs9/memfs/base_file_handle.cpp
  vfs9/memfs/stream_file_handle.cpp
  vfs9/memfs/seq_stream_file_handle.cpp
  vfs9/memfs/mapped_file_handle.cpp
  vfs9/memfs/scalar_file_handle.cpp
  vfs9/localfs/localfs.cpp
  vfs9/localfs/base_file_handle.cpp
  vfs9/localfs/stream_file_handle.cpp
  vfs9/localfs/seq_stream_file_handle.cpp
  vfs9/localfs/mapped_file_handle.cpp
  vfs9/detail/file_handle.cpp
  vfs9/detail/fs.cpp
)

target_sources(vfs9_static_lib PUBLIC ${HEADER_FILES})
target_sources(vfs9_static_lib PRIVATE ${SOURCE_FILES})

# CLI header files
set(
  CLI_HEADER_FILES
  vfs9_cli/cli.hpp
  vfs9_cli/command.hpp
  vfs9_cli/commands/bind.hpp
  vfs9_cli/commands/cd.hpp
  vfs9_cli/commands/exit.hpp
  vfs9_cli/commands/help.hpp
  vfs9_cli/commands/ls.hpp
  vfs9_cli/commands/mkdir.hpp
  vfs9_cli/commands/mount.hpp
  vfs9_cli/commands/pwd.hpp
  vfs9_cli/commands/unmount.hpp
  vfs9_cli/commands/unmount_all.hpp
)

# CLI source files
set(
  CLI_SOURCE_FILES
  vfs9_cli/main.cpp
  vfs9_cli/cli.cpp
  vfs9_cli/command.cpp
  vfs9_cli/commands/bind.cpp
  vfs9_cli/commands/cd.cpp
  vfs9_cli/commands/exit.cpp
  vfs9_cli/commands/help.cpp
  vfs9_cli/commands/ls.cpp
  vfs9_cli/commands/mkdir.cpp
  vfs9_cli/commands/mount.cpp
  vfs9_cli/commands/pwd.cpp
  vfs9_cli/commands/unmount.cpp
  vfs9_cli/commands/unmount_all.cpp
)

add_executable(vfs9_cli ${CLI_SOURCE_FILES} ${CLI_HEADER_FILES})
add_executable(vfs9::cli ALIAS vfs9_cli)
target_link_libraries(vfs9_cli PRIVATE vfs9_static_lib Boost::program_options linenoise)
set_target_properties(vfs9_cli PROPERTIES OUTPUT_NAME "vfs9-cli")
