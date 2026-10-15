/**
 * @file seq_stream_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Sequential stream file handle
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

#include "vfs9/buffer.hpp"
#include "vfs9/detail/type_traits.hpp"
#include "vfs9/file.hpp"
#include "vfs9/zipfs/base_file_handle.hpp"

namespace vfs9 {
namespace zipfs_impl {

/**
 * @brief Sequential stream file handle backed by a zip entry
 *
 *  Reading decompresses the entry on the fly. Seeking forward skips decompressed bytes; seeking
 *  backward (or to the beginning / end) closes and re-opens the entry and then skips forward, so it
 *  re-decompresses from the entry start. Writing is unsupported (the archive is read-only).
 *
 */
class seq_stream_file_handle : public base_file_handle {
public:
  /**
   * @brief Construct a new seq stream file handle object
   *
   * @param zip_path Path of the backing zip archive
   * @param path Path relative to the file system root
   * @param entry_name Entry name inside the zip archive
   * @param size Uncompressed file size
   */
  seq_stream_file_handle(std::filesystem::path zip_path,
                         std::filesystem::path path,
                         std::string entry_name,
                         size_t size);

  ~seq_stream_file_handle() override;

  seq_stream_file_handle(const seq_stream_file_handle&) = delete;
  seq_stream_file_handle(seq_stream_file_handle&&) = delete;

  seq_stream_file_handle& operator=(const seq_stream_file_handle&) = delete;
  seq_stream_file_handle& operator=(seq_stream_file_handle&&) = delete;

  /**
   * @brief File max size
   *
   * @return size_t
   */
  size_t max_size() const noexcept;

  /**
   * @brief Get read offset
   *
   * @return size_t
   */
  size_t tell_read() const noexcept;

  /**
   * @brief Get write offset
   *
   * @return size_t
   */
  size_t tell_write() const noexcept;

  /**
   * @brief Whether the file is seekable or not
   *
   * @return bool
   */
  bool seekable() const noexcept;

  /**
   * @brief Set read offset
   *
   * @param offset Offset
   * @param whence Seek whence
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> seek_read(int64_t offset, seq_stream_file::seek_whence whence) noexcept;

  /**
   * @brief Set write offset, always unsupported
   *
   * @param offset Offset
   * @param whence Seek whence
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> seek_write(int64_t offset, seq_stream_file::seek_whence whence) noexcept;

  /**
   * @brief Read from stream
   *
   * @param buffers Read to buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> read(mutable_buffer_sequence buffers) const noexcept;

  /**
   * @brief Write to stream, always unsupported
   *
   * @param buffers Write from buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> write(const_buffer_sequence buffers) const noexcept;

private:
  /**
   * @brief Close and re-open the entry, resetting the read position to the beginning
   *
   * @return std::error_code
   */
  std::error_code reopen_entry() noexcept;

  /**
   * @brief Skip forward by decompressing and discarding `count` bytes
   *
   * @param count Number of bytes to skip
   * @return std::error_code
   */
  std::error_code skip_forward(size_t count) noexcept;

  void* p_reader_ = nullptr;                 // mz_zip_reader instance
  std::string entry_name_;                   // Entry name inside the zip archive
  mutable size_t read_cursor_ = 0;            // Read cursor
  mutable size_t write_cursor_ = 0;           // Write cursor
  std::error_code error_;                    // Error captured while opening the entry
};

static_assert(detail::is_fs_seq_stream_file_handle<seq_stream_file_handle>);

} // namespace zipfs_impl
} // namespace vfs9
