/**
 * @file file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File handle
 * @version 0.1
 * @date 2026-10-11
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <any>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <system_error>
#include <typeinfo>
#include <utility>

#include "vfs9/detail/type_traits.hpp"
#include "vfs9/file.hpp"
#include "vfs9/fs.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief Base file handle
 *
 */
class base_file_handle {
public:
  /**
   * @brief Construct a new base file handle object
   *
   * @param open_mode File open mode
   */
  base_file_handle(file_open_mode open_mode) : open_mode_(open_mode) {}

  base_file_handle(const base_file_handle&) = delete;
  base_file_handle(base_file_handle&&) = delete;

  virtual ~base_file_handle();

  base_file_handle& operator=(const base_file_handle&) = delete;
  base_file_handle& operator=(base_file_handle&&) = delete;

  /**
   * @brief Get open mode
   *
   * @return file_open_mode
   */
  inline file_open_mode open_mode() const noexcept { return open_mode_; }

  /**
   * @brief Get node info
   *
   * @return node_info
   */
  virtual node_info get_info() const noexcept = 0;

private:
  file_open_mode open_mode_;
};

/**
 * @brief Stream file handle
 *
 */
class stream_file_handle : public base_file_handle {
public:
  using base_type = base_file_handle;

  using base_type::base_file_handle;

  ~stream_file_handle() override;

  /**
   * @brief File max size
   *
   * @return size_t
   */
  virtual size_t max_size() const = 0;

  /**
   * @brief Read from file
   *
   * @return std::expected<size_t, std::error_code>
   */
  virtual std::expected<size_t, std::error_code> read(size_t offset,
                                                      mutable_buffer_sequence buffers) const noexcept = 0;

  /**
   * @brief Write to file
   *
   * @return std::expected<size_t, std::error_code>
   */
  virtual std::expected<size_t, std::error_code> write(size_t offset, const_buffer_sequence buffers) const noexcept = 0;

private:
  template <is_fs_stream_file_handle T, typename... Ts>
  friend std::unique_ptr<stream_file_handle> make_stream_file_handle(file_open_mode open_mode, Ts&&... args);

  template <typename T>
    requires is_fs_stream_file_handle<T>
  class impl;
};

/**
 * @brief Stream file handle implementation
 *
 * @tparam T
 */
template <typename T>
  requires is_fs_stream_file_handle<T>
class stream_file_handle::impl : public stream_file_handle {
public:
  using base_type = stream_file_handle;

  template <typename... Ts>
  impl(file_open_mode open_mode, Ts&&... args) : base_type(open_mode), impl_(std::forward<Ts>(args)...) {}

  node_info get_info() const noexcept override { return impl_.get_info(); }

  size_t max_size() const override { return impl_.max_size(); }

  std::expected<size_t, std::error_code> read(size_t offset, mutable_buffer_sequence buffers) const noexcept override {
    return impl_.read(offset, buffers);
  }

  std::expected<size_t, std::error_code> write(size_t offset, const_buffer_sequence buffers) const noexcept override {
    return impl_.write(offset, buffers);
  }

private:
  T impl_;
};

/**
 * @brief Make a concrete stream file handle
 *
 * @tparam Handle implementation type
 * @tparam Ts Implementation constructor argument types
 * @param open_mode File open mode
 * @param args Implementation constructor arguments
 * @return std::unique_ptr<stream_file_handle>
 */
template <is_fs_stream_file_handle T, typename... Ts>
inline std::unique_ptr<stream_file_handle> make_stream_file_handle(file_open_mode open_mode, Ts&&... args) {
  return std::make_unique<stream_file_handle::impl<T>>(open_mode, std::forward<Ts>(args)...);
}

/**
 * @brief Sequential stream file handle
 *
 */
class seq_stream_file_handle : public base_file_handle {
public:
  using base_type = base_file_handle;

  using base_type::base_file_handle;

  ~seq_stream_file_handle() override;

  /**
   * @brief File max size
   *
   * @return size_t
   */
  virtual size_t max_size() const = 0;

  /**
   * @brief Get read offset
   *
   * @return size_t
   */
  virtual size_t tell_read() const noexcept = 0;

  /**
   * @brief Get write offset
   *
   * @return size_t
   */
  virtual size_t tell_write() const noexcept = 0;

  /**
   * @brief Whether the file is seekable or not
   *
   * @return bool
   */
  virtual bool seekable() const noexcept = 0;

  /**
   * @brief Set read offset
   *
   * @return std::expected<void, std::error_code>
   */
  virtual std::expected<void, std::error_code> seek_read(int64_t offset,
                                                         seq_stream_file::seek_whence whence) noexcept = 0;

  /**
   * @brief Set write offset
   *
   * @return std::expected<void, std::error_code>
   */
  virtual std::expected<void, std::error_code> seek_write(int64_t offset,
                                                          seq_stream_file::seek_whence whence) noexcept = 0;

  /**
   * @brief Read from stream
   *
   * @return std::expected<size_t, std::error_code>
   */
  virtual std::expected<size_t, std::error_code> read(mutable_buffer_sequence buffers) noexcept = 0;

  /**
   * @brief Write to stream
   *
   * @return std::expected<size_t, std::error_code>
   */
  virtual std::expected<size_t, std::error_code> write(const_buffer_sequence buffers) noexcept = 0;

private:
  template <is_fs_seq_stream_file_handle T, typename... Ts>
  friend std::unique_ptr<seq_stream_file_handle> make_seq_stream_file_handle(file_open_mode open_mode, Ts&&... args);

  template <typename T>
    requires is_fs_seq_stream_file_handle<T>
  class impl;
};

/**
 * @brief Sequential stream file handle implementation
 *
 * @tparam T
 */
template <typename T>
  requires is_fs_seq_stream_file_handle<T>
class seq_stream_file_handle::impl : public seq_stream_file_handle {
public:
  using base_type = seq_stream_file_handle;

  template <typename... Ts>
  impl(file_open_mode open_mode, Ts&&... args) : base_type(open_mode), impl_(std::forward<Ts>(args)...) {}

  node_info get_info() const noexcept override { return impl_.get_info(); }

  size_t max_size() const override { return impl_.max_size(); }

  size_t tell_read() const noexcept override { return impl_.tell_read(); }

  size_t tell_write() const noexcept override { return impl_.tell_write(); }

  bool seekable() const noexcept override { return impl_.seekable(); }

  std::expected<void, std::error_code> seek_read(int64_t offset,
                                                 seq_stream_file::seek_whence whence) noexcept override {
    return impl_.seek_read(offset, whence);
  }

  std::expected<void, std::error_code> seek_write(int64_t offset,
                                                  seq_stream_file::seek_whence whence) noexcept override {
    return impl_.seek_write(offset, whence);
  }

  std::expected<size_t, std::error_code> read(mutable_buffer_sequence buffers) noexcept override {
    return impl_.read(buffers);
  }

  std::expected<size_t, std::error_code> write(const_buffer_sequence buffers) noexcept override {
    return impl_.write(buffers);
  }

private:
  T impl_;
};

/**
 * @brief Make a concrete sequential stream file handle
 *
 * @tparam Handle implementation type
 * @tparam Ts Implementation constructor argument types
 * @param open_mode File open mode
 * @param args Implementation constructor arguments
 * @return std::unique_ptr<seq_stream_file_handle>
 */
template <is_fs_seq_stream_file_handle T, typename... Ts>
inline std::unique_ptr<seq_stream_file_handle> make_seq_stream_file_handle(file_open_mode open_mode, Ts&&... args) {
  return std::make_unique<seq_stream_file_handle::impl<T>>(open_mode, std::forward<Ts>(args)...);
}

/**
 * @brief Mapped file handle
 *
 */
class mapped_file_handle : public base_file_handle {
public:
  using base_type = base_file_handle;

  using base_type::base_file_handle;

  ~mapped_file_handle() override;

  /**
   * @brief File max size
   *
   * @return size_t
   */
  virtual size_t max_size() const = 0;

  /**
   * @brief Get mapped data
   *
   * @return std::span<std::byte>
   */
  virtual std::span<std::byte> data() noexcept = 0;

  /**
   * @brief Get mapped data
   *
   * @return std::span<const std::byte>
   */
  virtual std::span<const std::byte> data() const noexcept = 0;

  /**
   * @brief Get address space reservation
   *
   * @return size_t
   */
  virtual size_t reservation() const noexcept = 0;

  /**
   * @brief Get offset into the backing file where the mapping begins
   *
   * @return size_t
   */
  virtual size_t starting_offset() const noexcept = 0;

private:
  template <is_fs_mapped_file_handle T, typename... Ts>
  friend std::unique_ptr<mapped_file_handle> make_mapped_file_handle(file_open_mode open_mode, Ts&&... args);

  template <typename T>
    requires is_fs_mapped_file_handle<T>
  class impl;
};

/**
 * @brief Mapped file handle implementation
 *
 * @tparam T
 */
template <typename T>
  requires is_fs_mapped_file_handle<T>
class mapped_file_handle::impl : public mapped_file_handle {
public:
  using base_type = mapped_file_handle;

  template <typename... Ts>
  impl(file_open_mode open_mode, Ts&&... args) : base_type(open_mode), impl_(std::forward<Ts>(args)...) {}

  node_info get_info() const noexcept override { return impl_.get_info(); }

  size_t max_size() const override { return impl_.max_size(); }

  std::span<std::byte> data() noexcept override { return impl_.data(); }

  std::span<const std::byte> data() const noexcept override { return impl_.data(); }

  size_t reservation() const noexcept override { return impl_.reservation(); }

  size_t starting_offset() const noexcept override { return impl_.starting_offset(); }

private:
  T impl_;
};

/**
 * @brief Make a concrete mapped file handle
 *
 * @tparam Handle implementation type
 * @tparam Ts Implementation constructor argument types
 * @param open_mode File open mode
 * @param args Implementation constructor arguments
 * @return std::unique_ptr<mapped_file_handle>
 */
template <is_fs_mapped_file_handle T, typename... Ts>
inline std::unique_ptr<mapped_file_handle> make_mapped_file_handle(file_open_mode open_mode, Ts&&... args) {
  return std::make_unique<mapped_file_handle::impl<T>>(open_mode, std::forward<Ts>(args)...);
}

/**
 * @brief Scalar file handle
 *
 */
class scalar_file_handle : public base_file_handle {
public:
  using base_type = base_file_handle;

  using base_type::base_file_handle;

  ~scalar_file_handle() override;

  /**
   * @brief Get scalar value type
   *
   * @return const std::type_info&
   */
  virtual const std::type_info& type() const noexcept = 0;

  /**
   * @brief Read scalar value
   *
   * @return std::expected<std::any, std::error_code>
   */
  virtual std::expected<std::any, std::error_code> read() const noexcept = 0;

  /**
   * @brief Write scalar value
   *
   * @return std::expected<void, std::error_code>
   */
  virtual std::expected<void, std::error_code> write(std::any value) noexcept = 0;

private:
  template <is_fs_scalar_file_handle T, typename... Ts>
  friend std::unique_ptr<scalar_file_handle> make_scalar_file_handle(file_open_mode open_mode, Ts&&... args);

  template <typename T>
    requires is_fs_scalar_file_handle<T>
  class impl;
};

/**
 * @brief Scalar file handle implementation
 *
 * @tparam T
 */
template <typename T>
  requires is_fs_scalar_file_handle<T>
class scalar_file_handle::impl : public scalar_file_handle {
public:
  using base_type = scalar_file_handle;

  template <typename... Ts>
  impl(file_open_mode open_mode, Ts&&... args) : base_type(open_mode), impl_(std::forward<Ts>(args)...) {}

  node_info get_info() const noexcept override { return impl_.get_info(); }

  const std::type_info& type() const noexcept override { return impl_.type(); }

  std::expected<std::any, std::error_code> read() const noexcept override { return impl_.read(); }

  std::expected<void, std::error_code> write(std::any value) noexcept override { return impl_.write(std::move(value)); }

private:
  T impl_;
};

/**
 * @brief Make a concrete scalar file handle
 *
 * @tparam Handle implementation type
 * @tparam Ts Implementation constructor argument types
 * @param open_mode File open mode
 * @param args Implementation constructor arguments
 * @return std::unique_ptr<scalar_file_handle>
 */
template <is_fs_scalar_file_handle T, typename... Ts>
inline std::unique_ptr<scalar_file_handle> make_scalar_file_handle(file_open_mode open_mode, Ts&&... args) {
  return std::make_unique<scalar_file_handle::impl<T>>(open_mode, std::forward<Ts>(args)...);
}

/**
 * @brief Control file handle
 *
 */
class control_file_handle : public base_file_handle {
public:
  using base_type = base_file_handle;

  using base_type::base_file_handle;

  ~control_file_handle() override;

  /**
   * @brief Execute command
   *
   * @param command Command to execute
   * @return std::expected<void, std::error_code>
   */
  virtual std::expected<void, std::error_code> execute(std::string_view command) noexcept = 0;

private:
  template <is_fs_control_file_handle T, typename... Ts>
  friend std::unique_ptr<control_file_handle> make_control_file_handle(file_open_mode open_mode, Ts&&... args);

  template <typename T>
    requires is_fs_control_file_handle<T>
  class impl;
};

/**
 * @brief Control file handle implementation
 *
 * @tparam T
 */
template <typename T>
  requires is_fs_control_file_handle<T>
class control_file_handle::impl : public control_file_handle {
public:
  using base_type = control_file_handle;

  template <typename... Ts>
  impl(file_open_mode open_mode, Ts&&... args) : base_type(open_mode), impl_(std::forward<Ts>(args)...) {}

  node_info get_info() const noexcept override { return impl_.get_info(); }

  std::expected<void, std::error_code> execute(std::string_view command) noexcept override {
    return impl_.execute(command);
  }

private:
  T impl_;
};

/**
 * @brief Make a concrete control file handle
 *
 * @tparam Handle implementation type
 * @tparam Ts Implementation constructor argument types
 * @param open_mode File open mode
 * @param args Implementation constructor arguments
 * @return std::unique_ptr<control_file_handle>
 */
template <is_fs_control_file_handle T, typename... Ts>
inline std::unique_ptr<control_file_handle> make_control_file_handle(file_open_mode open_mode, Ts&&... args) {
  return std::make_unique<control_file_handle::impl<T>>(open_mode, std::forward<Ts>(args)...);
}

} // namespace detail
} // namespace vfs9
