# Local file system

Local file system implements a mapping to operation system's local file system.

There're some limitations of local file system:

- Only directory and regular file are visible, file with any other types are ignored which means they won't be visible by list / exists / status / open_file / ...
- Support placeholder file (which will map to a zero-size file in operation system's file system)
- Scalar & control file are not supported
