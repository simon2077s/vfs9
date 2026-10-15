/**
 * @file fs.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief File system, this is a file system implementation facade and supports ref tracing.
 * @version 0.1
 * @date 2026-08-30
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <atomic>
#include <expected>
#include <memory>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

#include <boost/describe.hpp>
#include <boost/mp11.hpp>

#include "vfs9/detail/type_traits.hpp"
#include "vfs9/fs.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief File system, wraps any file system implementation
 *
 */
class file_system {
public:
  file_system() : id_(++next_id_) {}

  virtual ~file_system() = default;

  file_system(const file_system&) = delete;
  file_system(file_system&&) = delete;

  file_system& operator=(const file_system&) = delete;
  file_system& operator=(file_system&&) = delete;

  inline uint32_t id() const noexcept { return id_; }

  virtual bool exists(const std::filesystem::path& path) = 0;

  virtual bool exists(const std::filesystem::path& path, node_type nt) = 0;

  virtual bool exists(const std::filesystem::path& path, std::string_view name) = 0;

  virtual bool exists(const std::filesystem::path& path, std::string_view name, node_type nt) = 0;

  virtual bool exists(const std::filesystem::path& path,
                      const std::filesystem::path::const_iterator& begin,
                      const std::filesystem::path::const_iterator& end) = 0;

  virtual bool exists(const std::filesystem::path& path,
                      const std::filesystem::path::const_iterator& begin,
                      const std::filesystem::path::const_iterator& end,
                      node_type nt) = 0;

  virtual node_status status(const std::filesystem::path& path) = 0;

  virtual node_info get_info(const std::filesystem::path& path) = 0;

  virtual std::expected<void, std::error_code> list(const std::filesystem::path& path,
                                                    std::vector<node_entry>& out) = 0;

  virtual std::expected<void, std::error_code> create_directory(const std::filesystem::path& path) = 0;

  virtual std::expected<void, std::error_code> create_directory(const std::filesystem::path& path, bool recursive) = 0;

  virtual std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                           placeholder_file_create_spec spec) = 0;

  virtual std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                           regular_file_create_spec spec) = 0;

  virtual std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                           scalar_file_create_spec spec) = 0;

  virtual std::expected<void, std::error_code> remove(const std::filesystem::path& path) = 0;

  virtual std::expected<void, std::error_code> remove(const std::filesystem::path& path, bool recursive) = 0;

  virtual std::expected<std::shared_ptr<stream_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                                 stream_file_open_options options) = 0;

  virtual std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
  open_file(const std::filesystem::path& path, seq_stream_file_open_options options) = 0;

  virtual std::expected<std::shared_ptr<mapped_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                                 mapped_file_open_options options) = 0;

  virtual std::expected<std::shared_ptr<scalar_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                                 scalar_file_open_options options) = 0;

  virtual std::expected<std::shared_ptr<control_file>, std::error_code>
  open_file(const std::filesystem::path& path, control_file_open_options options) = 0;

private:
  template <typename T, typename... Ts>
  friend std::expected<std::shared_ptr<file_system>, std::error_code> make_file_system(T&& params, Ts&&... args);

  template <typename T>
    requires(is_fs<T>)
  class impl;

  uint32_t id_; // Starts from 1

  // Global file system id
  static std::atomic<uint32_t> next_id_;
};

template <typename T>
  requires(is_fs<T>)
class file_system::impl : public file_system {
public:
  using fs_type = T;

  /**
   * @brief Construct a new impl object
   *
   * @tparam Ts Argument types of file system implementation constructor
   * @param args Arguments of file system implementation constructor
   */
  template <typename... Ts>
  impl(Ts&&... args) : inited_(false), impl_(std::forward<Ts>(args)...) {}

  impl(const impl&) = delete;
  impl(impl&&) = delete;

  ~impl() = default;

  impl& operator=(const impl&) = delete;
  impl& operator=(impl&&) = delete;

  /**
   * @brief Initialize the file system
   *
   * @param params File system parameter
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> init(fs_type params) noexcept {
    auto result = impl_.init(std::move(params));
    if (result) {
      inited_ = true;
    }
    return result;
  }

  bool exists(const std::filesystem::path& path) override { return impl_.exists(path); }

  bool exists(const std::filesystem::path& path, node_type nt) override { return impl_.exists(path, nt); }

  bool exists(const std::filesystem::path& path, std::string_view name) override { return impl_.exists(path, name); }

  bool exists(const std::filesystem::path& path, std::string_view name, node_type nt) override {
    return impl_.exists(path, name, nt);
  }

  bool exists(const std::filesystem::path& path,
              const std::filesystem::path::const_iterator& begin,
              const std::filesystem::path::const_iterator& end) override {
    return impl_.exists(path, begin, end);
  }

  bool exists(const std::filesystem::path& path,
              const std::filesystem::path::const_iterator& begin,
              const std::filesystem::path::const_iterator& end,
              node_type nt) override {
    return impl_.exists(path, begin, end, nt);
  }

  node_status status(const std::filesystem::path& path) override { return impl_.status(path); }

  node_info get_info(const std::filesystem::path& path) override { return impl_.get_info(path); }

  std::expected<void, std::error_code> list(const std::filesystem::path& path, std::vector<node_entry>& out) override {
    return impl_.list(path, out);
  }

  std::expected<void, std::error_code> create_directory(const std::filesystem::path& path) override {
    return impl_.create_directory(path);
  }

  std::expected<void, std::error_code> create_directory(const std::filesystem::path& path, bool recursive) override {
    return impl_.create_directory(path, recursive);
  }

  std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                   placeholder_file_create_spec spec) override {
    return impl_.create_file(path, spec);
  }

  std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                   regular_file_create_spec spec) override {
    return impl_.create_file(path, spec);
  }

  std::expected<void, std::error_code> create_file(const std::filesystem::path& path,
                                                   scalar_file_create_spec spec) override {
    return impl_.create_file(path, spec);
  }

  std::expected<void, std::error_code> remove(const std::filesystem::path& path) override { return impl_.remove(path); }

  std::expected<void, std::error_code> remove(const std::filesystem::path& path, bool recursive) override {
    return impl_.remove(path, recursive);
  }

  std::expected<std::shared_ptr<stream_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                         stream_file_open_options options) override {
    return impl_.open_file(path, options);
  }

  std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
  open_file(const std::filesystem::path& path, seq_stream_file_open_options options) override {
    return impl_.open_file(path, options);
  }

  std::expected<std::shared_ptr<mapped_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                         mapped_file_open_options options) override {
    return impl_.open_file(path, options);
  }

  std::expected<std::shared_ptr<scalar_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                         scalar_file_open_options options) override {
    return impl_.open_file(path, options);
  }

  std::expected<std::shared_ptr<control_file>, std::error_code> open_file(const std::filesystem::path& path,
                                                                          control_file_open_options options) override {
    return impl_.open_file(path, options);
  }

private:
  using impl_type = fs_impl_type<T>;

  bool inited_;
  impl_type impl_;
};

/**
 * @brief Create a new file system
 *
 * @tparam T File system type (aka. File system parameter)
 * @tparam Ts Argument types of file system implementation constructor
 * @param params File system parameters
 * @param args Arguments of file system implementation constructor
 * @return std::shared_ptr<file_system>
 */
template <typename T, typename... Ts>
std::expected<std::shared_ptr<file_system>, std::error_code> make_file_system(T&& params, Ts&&... args) {
  auto ptr = std::make_shared<file_system::impl<std::remove_cvref_t<T>>>(std::forward<Ts>(args)...);
  auto init_result = ptr->init(std::forward<T>(params));
  if (!init_result) {
    return std::unexpected(init_result.error());
  }
  return ptr;
}

} // namespace detail
} // namespace vfs9
