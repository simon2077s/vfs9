# Memory file system

Memory file system implements a in - memory file system which supports:

- Node tree management like: create / remove / list directories and files
- Placeholder file creation
- Regular file creation and read / write
- Scalar file creation and read / write

Memory file system doesn't support control file.

Since memory file system is totally in memory, there're some limitations of regular file:

- Since the memory file implementation is based on llfio `section_handle`, it's recommended to specify the initial file size when creating the file otherwise the initial file size will depend on implementation
- The file will auto increase its max size when necessary, but this will consume lots of memory
