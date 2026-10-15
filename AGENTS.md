# AGENTS.md

`vfs9` is a header-mostly C++ virtual filesystem library inspired by Plan 9 ("everything is a file"). It mounts local/archive filesystems and functional subsystems into one namespace. Public umbrella header: `src/vfs9.hpp`.

Status: **early-stage but building.** `vfs9::static_lib` and all tests build and pass. Three backends are implemented: `memfs` (`src/vfs9/memfs/`, in-memory, on llfio `section_handle` + `map_handle` for regular files and `std::any` for scalar files), `localfs` (`src/vfs9/localfs/`, maps an OS directory; metadata via `std::filesystem`, file i/o via llfio `file_handle` / `mapped_file_handle`; only directories + regular files visible; symlinks followed; scalar/control unsupported), and `zipfs` (`src/vfs9/zipfs/`, read-only zip archive via minizip-ng; only directories + regular files; only `seq_stream` files can be opened; no placeholder/scalar/control, no stream/mapped; seek reopens the entry). The `detail::file_system::impl<T>` facade (`detail/fs.hpp`) fully implements the `is_fs` interface (delegating to `fs_impl<T>`), so `make_file_system` / `vnamespace` link. `vnamespace` exposes the full path-based file-system API: queries (`status`/`exists`/`get_info`/`list`), mutation (`create_directory`/`create_file`/`remove`), `open_file` (5 kinds) and the `read_all`/`write_all`/`read_scalar`/`write_scalar`/`execute` helpers.

## Build

Requires CMake >= 4.0, C++23, Boost (`program_options` + `json`), and **llfio** (installed via vcpkg). Use the `vcpkg` configure preset, which reads the toolchain from the `VCPKG_ROOT` environment variable (so `find_package(llfio CONFIG)` works):

```sh
cmake --preset vcpkg -DVFS9_BUILD_BENCHMARKS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

- Options: `VFS9_BUILD_TESTS` (ON), `VFS9_BUILD_BENCHMARKS` (ON).
- `src/CMakeLists.txt` links `llfio::sl` (plus `Threads`, found explicitly). Do **not** use `llfio::ntkernel-error-category::sl` (not exported on this install; Windows-only).
- Tests use Catch2 v3 via `catch_discover_tests`. `test/node_tree.cpp` is header-only (no lib link); `test/memfs.cpp`, `test/localfs.cpp`, `test/vns.cpp` link `vfs9::static_lib`.
- `src/CMakeLists.txt` fetches **cpp-linenoise** (`FetchContent`, pinned commit `b0e8b88…`, target `linenoise`) for the CLI; `benchmark/` only fetches Google Benchmark and has no sources.
- `src/CMakeLists.txt` also `find_package(minizip-ng CONFIG REQUIRED)` and links `MINIZIP::minizip-ng` (PRIVATE) for `zipfs`; install it with `vcpkg install minizip-ng[zlib]`.
- Targets: `vfs9::static_lib`, `vfs9::cli`, `vfs9_test_node_tree`, `vfs9_test_memfs`, `vfs9_test_localfs`, `vfs9_test_vns`, `vfs9_test_zipfs`.
- `vfs9::cli` (built as `vfs9-cli`) is an interactive REPL (`src/vfs9_cli/`); it reads lines with **cpp-linenoise** (fetched via `FetchContent`, target `linenoise`; cursor movement + up/down history, history fed by `AddHistory`), parses global/`--help` options and per-command args with Boost.`program_options` (linked to the CLI target only) and dispatches subcommands (each in its own `src/vfs9_cli/commands/<name>.{hpp,cpp}`). The CLI tracks a `current_path_` (starts at `/`); every path argument is resolved by `cli::resolve_path` (absolute or relative), with `cd`/`pwd` commands and `ls [path]` defaulting to the current directory.

## Known gotchas

- C++ standard is **23** (`CMakeLists.txt`; `.clangd` forces it too). The code uses `std::expected`.
- **llfio namespace is versioned.** Use `LLFIO_V2_NAMESPACE` or add `namespace llfio = LLFIO_V2_NAMESPACE;`. `llfio::error_info::ec` is private — convert errors with `make_error_code(result.error())` (ADL).
- FS plugin model: specialize `vfs9::detail::fs_impl<Identity>` exposing `using type = <impl>;`, and the impl must satisfy the `is_fs<T>` concept (`detail/type_traits.hpp`). The memfs impl lives in `vfs9::memfs_impl::fs_impl`.
- File handles: the fs-side handle type must directly satisfy `is_fs_{stream,seq_stream,mapped,scalar,control}_file_handle<T>`. Public file classes (`stream_file`, …) wrap it via `detail::make_<kind>_file_handle<T>(mode, args...)` (free functions; the `impl<T>` wrapper is private + friend).
- The seq-stream handle's `read`/`write` are **const** per the concept (cursors are `mutable`); `seek_read`/`seek_write` are non-const.
- `std::filesystem::path` iterators are owned by the path; never build `begin()`/`end()` from separate temporaries. `node_tree` path APIs take `(const Iterator& begin, Iterator& end)` and skip a leading `/`.
- `operator<<(std::ostream&, const std::filesystem::path&)` writes a **quoted/escaped** path (e.g. `"/a"` -> `"\"/a\""`); use `path.string()` / `path.c_str()` when assembling values (e.g. mount-table keys).
- `README.md` is partly stale (names `virtual_file_system` / `any_fs`); trust the code. README/design notes are in Chinese.

## Architecture

- `vnamespace` (`src/vfs9/vns.{hpp,cpp}`) is the core type: mount / bind / unmount, path lookup, clone / shadow, freeze, plus the path-based FS API (queries, mutation, `open_file`, read/write/execute helpers); owns a `detail::mount_table` (`node_id` -> `mount_point`). `make_namespace(...)` builds one. A shadow's mount table points at its parent's (`from_parent` mount points are copy-on-write materialized before mutation); `clone()` flattens the parent chain into a standalone table.
- Write dispatch: queries pick the highest-priority (`entry_list()[0]`) mount entry; writes pick the first writable entry (`resolve_node_ref(..., true)`), and `create_*` walk up to the nearest existing directory so `recursive` can materialize missing components inside the target fs (`resolve_creation_target`). A mount entry's `writable` flag is carried through `lookup_path_result` so writes into a read-only entry fail. `remove` rejects a path that resolves to a mount point (`errc::unsupported`) — mount points are removed via `unmount`; it also refuses to remove a file that is currently open (`errc::file_in_use`) — memfs tracks a per-node open count, localfs tracks open fs-relative paths through a shared `open_registry`, and zipfs refuses all removal. The vfs9 `errc` enum has a `file_in_use` value for this.
- `node_id` = (fs_id, path-within-fs). Path resolution walks the mount table by **recorded path**, not node identity — a deliberate divergence from Plan 9 that also means **no move semantics**.
- `mount_point` is an ordered list of `mount_entry` (a union directory when size > 1); order = priority, per-entry `writable` gates writes.
- `detail::file_system` (`detail/fs.hpp`) is the type-erased facade (all `is_fs` methods are pure virtual and forwarded by `file_system::impl<T>`); `make_file_system(params, args...)` builds `file_system::impl<T>` and calls `init`.
- `detail::mount_table` (`detail/mount_table.hpp`) maps `node_id` -> `mount_point`; `lookup_nearest` builds candidate paths with `path.string()` (never `operator<<`, which would quote). `flatten()` merges the parent chain for `clone()`.
- FS concepts (`detail/type_traits.hpp`): `is_fs` (name / init / exists (6) / status / get_info / list / create_directory (2) / create_file (3 specs) / remove (2) / open_file (5 kinds)); `is_fs_*_file_handle`.
- `detail::node_tree` (`detail/node_tree.hpp`): move-only, non-thread-safe name -> node tree; `node_tree_node<Base>` derives from `Base`; `node_tree_node_map` is an array of 32 optionals promoted to `sv_map` above 32.
- `memfs` (`src/vfs9/memfs/`): `node_base` carries `file_type` + `size` + optional `section_handle`/`map_handle` (regular) or `std::any` (scalar); handles are `base_file_handle` + `stream`/`seq_stream`/`mapped`/`scalar`; `open_file` builds public file wrappers. Control files are unsupported (`errc::unsupported`).
- `localfs` (`src/vfs9/localfs/`): `localfs{ root }` maps fs paths onto `root` (absolute). Tree ops (exists/status/get_info/list/create_directory/remove) use `std::filesystem` and only surface directories + regular files (symlinks followed). `create_file`/`open_file` use llfio (`file_handle` for placeholder/stream/seq, `mapped_file_handle` for mapped); scalar and control are `errc::unsupported`. Handles mirror memfs (`base_file_handle` holds the local + fs path; stream/seq hold an llfio `file_handle`; mapped holds an llfio `mapped_file_handle`).
- `zipfs` (`src/vfs9/zipfs/`): `zipfs{ path }` (absolute path) reads the archive's table of contents once in `init` into a `detail::node_tree<zipfs_impl::node_base>` (each file node stores the uncompressed size); entry names are normalized (leading/trailing `/` stripped) and directories materialized via `recursive_set`. Read-only: `create_*`/`remove` and `stream`/`mapped`/`scalar`/`control` open are `errc::unsupported`. Only `seq_stream_file_handle` is supported; it owns a per-handle `mz_zip_reader`, reads by decompressing on the fly, and implements seek by skipping forward or closing+`locate_entry`+reopening the entry (so backward seek re-decompresses); `seekable()==true`, write/`seek_write` unsupported. `mz_util.hpp` maps `MZ_*` codes to `std::error_code`. Note: `mz_zip_rw.h` requires `mz.h`/`mz_strm.h`/`mz_zip.h` included first.
- `freeze()` (`detail/lock.hpp`, `freezable_mutex`) permanently disables mount/bind/create/remove and drops locking; no unfreeze.

## Conventions

- Namespace `vfs9`; internals in `vfs9::detail`. `#pragma once`. 2-space indent, 120-col, LLVM-based (`.clang-format`).
- Every file starts with the Doxygen header block (`@file @author @brief @version @date @copyright`); keep the file list in `src/CMakeLists.txt` in sync when adding headers/sources.
- Naming: `snake_case` for functions/types, trailing `_` for members, `p_` prefix for pointers, `c` prefix for const-ref params.
- Errors flow as `std::error_code` / `std::expected<T, std::error_code>`; `errc` is a `BOOST_DEFINE_ENUM_CLASS` in `error.hpp` (`boost/describe` for reflection).
