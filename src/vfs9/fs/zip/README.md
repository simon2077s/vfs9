# Zip

zip is a read-only file system backend that exposes the contents of a zip archive, implemented on
top of [minizip-ng](https://github.com/zlib-ng/minizip-ng).

## Parameters

```cpp
vfs9::zipfs params{ std::filesystem::path("/absolute/path/to/archive.zip") };
```

The archive path must be absolute and point to an existing regular file. On `init` the archive's
table of contents is read once into a node tree (directories + regular files, each file node stores
its uncompressed size); the archive itself is only reopened lazily when a file is opened.

## Capabilities

- Only directories and regular files are exposed. Placeholder, scalar and control files are not
  supported, and all mutating operations (`create_directory`, `create_file`, `remove`) return
  `errc::unsupported`.
- Files can only be opened as `seq_stream_file` (`seq_stream_file_open_options`). `stream_file`,
  `mapped_file`, `scalar_file` and `control_file` opens return `errc::unsupported`.
- Reading decompresses the entry on the fly. The `seq_stream_file` handle owns its own
  `mz_zip_reader`, so multiple open handles don't interfere.
- `seekable()` is `true` but seek is limited: a forward seek skips decompressed bytes; a backward
  seek (including rewinding to the beginning or seeking from the end) closes and re-opens the entry
  and then skips forward, i.e. it re-decompresses from the entry start (O(offset)). Writing and
  `seek_write` are `errc::unsupported`.

## Dependencies

Requires the vcpkg port `minizip-ng` (with the `zlib` feature to read deflate entries):
`vcpkg install minizip-ng[zlib]`. `mz_zip_rw.h` requires `mz.h`, `mz_strm.h` and `mz_zip.h` to be
included first.
