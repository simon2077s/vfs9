# pakfs

pakfs is a read-only file system backend for Quake / id Tech "PACK" archives (`.pak`), the format
used by Quake, Quake II, Quake III and other id Tech games. It has no external dependency.

## Format

A pak file is:

```
header:  char id[4];   // "PACK"
         int  diroffset; // little endian
         int  dirlength; // little endian
data:    raw (uncompressed) file payloads
dir:     dirlength / 64 entries, each:
           char name[56]; // '/' separated path, NUL padded
           int  filepos;  // little endian, offset of the payload
           int  filelen;  // little endian
```

## Parameters

```cpp
vfs9::pakfs params{ std::filesystem::path("/absolute/path/to/archive.pak") };
```

The archive path must be absolute and point to an existing regular file. On `init` the header and
directory are parsed once into a node tree (directories + regular files, each file node stores its
payload offset and size); the archive is reopened lazily when a file is opened.

## Capabilities

- Only directories and regular files are exposed. Placeholder, scalar and control files are not
  supported, and all mutating operations (`create_directory`, `create_file`, `remove`) return
  `errc::unsupported`.
- Entries are stored uncompressed, so files can be opened both as `stream_file` (random access) and
  as `seq_stream_file`; `seq_stream_file` supports full seeking (`seekable() == true`). `mapped_file`,
  `scalar_file` and `control_file` opens return `errc::unsupported`.
- Reads are clamped to the entry bounds, so they never cross into the next entry in the archive.
- Entry names are normalized: `\` becomes `/`, leading/trailing separators are stripped, and
  intermediate directories are materialized implicitly.
