/**
 * @file file.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File
 * @version 0.1
 * @date 2026-10-12
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <typeinfo>

#include "vfs9/buffer.hpp"
#include "vfs9/fs.hpp"

namespace vfs9 {

namespace detail {
class stream_file_handle;
class seq_stream_file_handle;
class mapped_file_handle;
class scalar_file_handle;
class control_file_handle;
} // namespace detail

/**
 * @brief File creation policy
 *
 *  Mirrors llfio::handle::creation.
 *
 */
enum class file_creation : uint8_t {
  open_existing,     // File must already exist
  only_if_not_exist, // File must not exist, it is created
  if_needed,         // Use the file if it exists, otherwise create it
  truncate_existing, // File must already exist and is truncated
  always_new,        // File is replaced with a new one if it exists, otherwise created
};

/**
 * @brief Kernel caching policy
 *
 *  Mirrors llfio::handle::caching.
 *
 */
enum class file_caching : uint8_t {
  none,               // No caching at all
  only_metadata,      // Cache metadata only
  reads,              // Cache reads only
  reads_and_metadata, // Cache reads and metadata only
  all,                // Cache reads, writes and metadata
  safety_barriers,    // Cache everything but issue safety barriers
  temporary,          // Cache everything, flush on last close if possible
};

/**
 * @brief Additional file behaviour flags
 *
 *  Mirrors llfio::handle::flag.
 *
 */
enum class file_flag : uint16_t {
  none = 0,
  unlink_on_first_close = 1u << 0,   // Unlink the file when the handle is closed
  disable_safety_barriers = 1u << 2, // Do not add implicit safety fsyncs
  disable_safety_unlinks = 1u << 3,  // Skip the inode check before unlinking
  disable_prefetching = 1u << 4,     // Disable OS read prefetching
  maximum_prefetching = 1u << 5,     // Maximize OS read prefetching
  multiplexable = 1u << 13,          // Allow multiplexable i/o
};

constexpr file_flag operator|(file_flag a, file_flag b) noexcept {
  return static_cast<file_flag>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

constexpr file_flag operator&(file_flag a, file_flag b) noexcept {
  return static_cast<file_flag>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

constexpr file_flag operator^(file_flag a, file_flag b) noexcept {
  return static_cast<file_flag>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
}

constexpr file_flag operator~(file_flag a) noexcept { return static_cast<file_flag>(~static_cast<uint16_t>(a)); }

inline file_flag& operator|=(file_flag& a, file_flag b) noexcept { return a = a | b; }

inline file_flag& operator&=(file_flag& a, file_flag b) noexcept { return a = a & b; }

/**
 * @brief File open mode
 *
 */
enum class file_open_mode : uint8_t {
  read,   // Read only
  write,  // Read and write
  append, // Write only, writes are always appended
};

/**
 * @brief Base of file open options
 *
 */
struct file_open_options_base {
  file_open_mode mode = file_open_mode::read; // Open mode
  file_caching caching = file_caching::all;   // Kernel caching policy
  file_flag flags = file_flag::none;          // Additional flags
};

/**
 * @brief Base of file create specs
 *
 */
struct file_create_spec_base {
  file_creation creation = file_creation::only_if_not_exist; // Creation policy
};

/**
 * @brief Placeholder file spec, used to create a placeholder file
 *
 */
struct placeholder_file_create_spec : file_create_spec_base {
  // No parameters
};

/**
 * @brief Regular file spec, used to create a regular file
 *
 */
struct regular_file_create_spec : file_create_spec_base {
  size_t initial_size = 0;            // Initial size
  std::optional<size_t> reserve_size; // Reserve size
};

/**
 * @brief Scalar file spec, used to create a scalar file
 *
 */
struct scalar_file_create_spec : file_create_spec_base {
  std::any init_value; // Initial value of the scalar file
};

/**
 * @brief File base
 *
 */
template <typename Handle>
class file_base {
public:
  using handle_type = Handle;

  /**
   * @brief Construct a new file base object
   *
   * @param open_mode File open mode
   * @param p_handle Handle shared pointer
   */
  file_base(file_open_mode open_mode, std::unique_ptr<handle_type> p_handle)
      : open_mode_(open_mode), p_handle_(std::move(p_handle)) {}

  file_base(const file_base&) = delete;
  file_base(file_base&&) = delete;

  ~file_base() = default;

  file_base& operator=(const file_base&) = delete;
  file_base& operator=(file_base&&) = delete;

  /**
   * @brief File open mode
   *
   * @return file_open_mode
   */
  file_open_mode open_mode() const noexcept { return p_handle_->open_mode(); }

  /**
   * @brief Get the node info
   *
   * @return node_info
   */
  node_info get_info() const noexcept { return p_handle_->get_info(); }

protected:
  /**
   * @brief Get file handle
   *
   * @return handle_type&
   */
  handle_type& handle() noexcept { return *p_handle_; }

  /**
   * @brief Get file handle
   *
   * @return const handle_type&
   */
  const handle_type& handle() const noexcept { return *p_handle_; }

private:
  file_open_mode open_mode_;
  std::unique_ptr<handle_type> p_handle_;
};

/**
 * @brief Stream file open options
 *
 */
struct stream_file_open_options : file_open_options_base {};

/**
 * @brief Stream file
 *
 */
class stream_file : public file_base<detail::stream_file_handle> {
public:
  using base_type = file_base<detail::stream_file_handle>;

  using base_type::file_base;

  /**
   * @brief Max file size
   *
   * @return size_t
   */
  size_t max_size() const noexcept;

  /**
   * @brief Read from file by an absolute offset
   *
   * @param offset Read from offset
   * @param buffers Read to buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> read(size_t offset, mutable_buffer_sequence buffers) const noexcept;

  /**
   * @brief Write to file by an absolute offset
   *
   * @param offset Write to offset
   * @param buffers Write from buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> write(size_t offset, const_buffer_sequence buffers) const noexcept;
};

/**
 * @brief Sequential stream file open options
 *
 */
struct seq_stream_file_open_options : file_open_options_base {};

/**
 * @brief Seq stream file
 *
 */
class seq_stream_file : public file_base<detail::seq_stream_file_handle> {
public:
  using base_type = file_base<detail::seq_stream_file_handle>;

  /**
   * @brief File seek whence
   *
   */
  enum class seek_whence {
    begin,   // Set offset from begining
    current, // Set offset from current position
    end,     // Set offset from end, always use a negative offset value
  };

  using base_type::file_base;

  /**
   * @brief Max file size
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
   * @param offset
   * @param whence
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> seek_read(int64_t offset, seq_stream_file::seek_whence whence) noexcept;

  /**
   * @brief Set write offset
   *
   * @param offset
   * @param whence
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> seek_write(int64_t offset, seq_stream_file::seek_whence whence) noexcept;

  /**
   * @brief Read from stream
   *
   * @param buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> read(mutable_buffer_sequence buffers) noexcept;

  /**
   * @brief Write to stream
   *
   * @param buffers
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code> write(const_buffer_sequence buffers) noexcept;
};

/**
 * @brief Mapped file open options
 *
 */
struct mapped_file_open_options : file_open_options_base {
  size_t reservation = 0; // Address space reservation, 0 means reserve the current file size
  size_t offset = 0;      // Offset into the backing file where the mapping begins
};

/**
 * @brief Mapped file
 *
 */
class mapped_file : public file_base<detail::mapped_file_handle> {
public:
  using base_type = file_base<detail::mapped_file_handle>;

  using base_type::file_base;

  /**
   * @brief Max file size
   *
   * @return size_t
   */
  size_t max_size() const noexcept;

  /**
   * @brief Get data
   *
   * @return std::span<std::byte>
   */
  std::span<std::byte> data() noexcept;

  /**
   * @brief Get data
   *
   * @return std::span<std::byte>
   */
  std::span<const std::byte> data() const noexcept;

  /**
   * @brief Get the address space reservation
   *
   * @return size_t
   */
  size_t reservation() const noexcept;

  /**
   * @brief Get the offset into the backing file where the mapping begins
   *
   * @return size_t
   */
  size_t starting_offset() const noexcept;
};

/**
 * @brief Scalar file open options
 *
 */
struct scalar_file_open_options : file_open_options_base {};

/**
 * @brief Scalar file
 *
 */
class scalar_file : public file_base<detail::scalar_file_handle> {
public:
  using base_type = file_base<detail::scalar_file_handle>;

  using base_type::file_base;

  /**
   * @brief Get type
   *
   * @return const std::type_info&
   */
  const std::type_info& type() const noexcept;

  /**
   * @brief Read scalar value
   *
   * @return std::expected<std::any, std::error_code>
   */
  std::expected<std::any, std::error_code> read() const noexcept;

  /**
   * @brief Write scalar value
   *
   * @param value Value to write
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> write(std::any value) noexcept;
};

/**
 * @brief Control file open options
 *
 */
struct control_file_open_options {
  file_flag flags = file_flag::none; // Additional flags
};

/**
 * @brief Control file
 *
 */
class control_file : public file_base<detail::control_file_handle> {
public:
  using base_type = file_base<detail::control_file_handle>;

  using base_type::file_base;

  /**
   * @brief Execute command
   *
   * @param command
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> execute(std::string_view command) noexcept;
};

} // namespace vfs9
