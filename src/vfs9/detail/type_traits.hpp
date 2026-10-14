/**
 * @file type_traits.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Type traits
 * @version 0.1
 * @date 2026-08-16
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <concepts>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>
#include <variant>
#include <vector>

#include <boost/describe.hpp>
#include <boost/mp11.hpp>

#include "vfs9/file.hpp"
#include "vfs9/fs.hpp"

namespace vfs9 {
namespace detail {

using fspath = std::filesystem::path;
using fspathcit = std::filesystem::path::const_iterator;

/**
 * @brief Convert tuple to variant. NOTE: This utility won't distinct tuple types.
 *
 * @tparam Ts
 */
template <typename T>
struct tuple_to_variant {};

template <typename... Ts>
struct tuple_to_variant<std::tuple<Ts...>> {
  using type = std::variant<Ts...>;
};

template <typename... Ts>
struct tuple_to_variant<boost::mp11::mp_list<Ts...>> {
  using type = std::variant<Ts...>;
};

template <typename... Ts>
struct tuple_to_variant<boost::describe::detail::list<Ts...>> {
  using type = std::variant<Ts...>;
};

template <typename T>
using tuple_to_variant_t = typename tuple_to_variant<T>::type;

template <typename T>
struct is_span : std::false_type {};

template <typename T>
struct is_span<std::span<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_span_v = is_span<T>::value;

template <typename T>
struct fs_impl {};

template <typename T>
using fs_impl_type = fs_impl<T>::type;

/**
 * @brief This concept defines the interface of fs_impl<T> which is the primary interface of file system
 *
 */
template <typename T, typename U = fs_impl_type<T>>
concept is_fs = requires(U t, const U& ct) {
  /**
   * @brief Must define a function:
   *
   *  std::string_view name() const noexcept;
   *
   */
  { ct.name() } noexcept -> std::same_as<std::string_view>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> init(T params) noexcept
   *
   */
  { t.init(std::declval<T>()) } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define functions:
   *
   *  bool exists(const std::filesystem::path& path) noexcept
   *  bool exists(const std::filesystem::path& path, node_type type) noexcept
   *  bool exists(const std::filesystem::path& path, std::string_view name) noexcept
   *  bool exists(const std::filesystem::path& path, std::string_view name, node_type type) noexcept
   *  bool exists(const std::filesystem::path& path,
   *              const std::filesystem::path::const_iterator& begin,
   *              const std::filesystem::path::const_iterator& end) noexcept
   *  bool exists(const std::filesystem::path& path,
   *              const std::filesystem::path::const_iterator& begin,
   *              const std::filesystem::path::const_iterator& end,
   *              node_type type) noexcept
   *
   *  NOTE: The `name` overloads check whether `path` is a directory containing a child named `name`. The iterator
   *  overloads resolve the path components in `[begin, end)` relative to `path`.
   */
  { ct.exists(std::declval<const fspath&>()) } noexcept -> std::same_as<bool>;
  { ct.exists(std::declval<const fspath&>(), std::declval<node_type>()) } noexcept -> std::same_as<bool>;
  { ct.exists(std::declval<const fspath&>(), std::declval<std::string_view>()) } noexcept -> std::same_as<bool>;
  {
    ct.exists(std::declval<const fspath&>(), std::declval<std::string_view>(), std::declval<node_type>())
  } noexcept -> std::same_as<bool>;
  {
    ct.exists(std::declval<const fspath&>(), std::declval<const fspathcit&>(), std::declval<const fspathcit&>())
  } noexcept -> std::same_as<bool>;
  {
    ct.exists(std::declval<const fspath&>(),
              std::declval<const fspathcit&>(),
              std::declval<const fspathcit&>(),
              std::declval<node_type>())
  } noexcept -> std::same_as<bool>;

  /**
   * @brief Must define a function:
   *
   *  node_status status(const std::filesystem::path& path) noexcept
   *
   */
  { ct.status(std::declval<const std::filesystem::path&>()) } noexcept -> std::same_as<node_status>;

  /**
   * @brief Must define a function:
   *
   *  node_info get_info(const std::filesystem::path& path) noexcept
   *
   */
  { ct.get_info(std::declval<const std::filesystem::path&>()) } noexcept -> std::same_as<node_info>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> list(const std::filesystem::path& path, std::vector<node_entry>&) noexcept
   *
   */
  {
    ct.list(std::declval<const std::filesystem::path&>(), std::declval<std::vector<node_entry>&>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> create_directory(const std::filesystem::path& path, bool recursive=false)
   * noexcept
   *
   */
  {
    t.create_directory(std::declval<const std::filesystem::path&>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;
  {
    t.create_directory(std::declval<const std::filesystem::path&>(), std::declval<bool>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> create_file(const std::filesystem::path& path, placeholder_file_create_spec
   * spec) noexcept
   *
   */
  {
    t.create_file(std::declval<const std::filesystem::path&>(), std::declval<placeholder_file_create_spec>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> create_file(const std::filesystem::path& path, regular_file_create_spec spec)
   * noexcept
   *
   */
  {
    t.create_file(std::declval<const std::filesystem::path&>(), std::declval<regular_file_create_spec>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> create_file(const std::filesystem::path& path, scalar_file_create_spec spec)
   * noexcept
   *
   */
  {
    t.create_file(std::declval<const std::filesystem::path&>(), std::declval<scalar_file_create_spec>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> remove(const std::filesystem::path& path, bool recursive=false) noexcept
   *
   */
  {
    t.remove(std::declval<const std::filesystem::path&>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;
  {
    t.remove(std::declval<const std::filesystem::path&>(), std::declval<bool>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<std::shared_ptr<stream_file>, std::error_code> open_file(const std::filesystem::path& path,
   * stream_file_open_options options) noexcept
   *
   */
  {
    t.open_file(std::declval<const std::filesystem::path&>(), std::declval<stream_file_open_options>())
  } noexcept -> std::same_as<std::expected<std::shared_ptr<stream_file>, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<std::shared_ptr<seq_stream_file>, std::error_code> open_file(const std::filesystem::path& path,
   * seq_stream_file_open_options options) noexcept
   *
   */
  {
    t.open_file(std::declval<const std::filesystem::path&>(), std::declval<seq_stream_file_open_options>())
  } noexcept -> std::same_as<std::expected<std::shared_ptr<seq_stream_file>, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<std::shared_ptr<mapped_file>, std::error_code> open_file(const std::filesystem::path& path,
   * mapped_file_open_options options) noexcept
   *
   */
  {
    t.open_file(std::declval<const std::filesystem::path&>(), std::declval<mapped_file_open_options>())
  } noexcept -> std::same_as<std::expected<std::shared_ptr<mapped_file>, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<std::shared_ptr<scalar_file>, std::error_code> open_file(const std::filesystem::path& path,
   * scalar_file_open_options options) noexcept
   *
   */
  {
    t.open_file(std::declval<const std::filesystem::path&>(), std::declval<scalar_file_open_options>())
  } noexcept -> std::same_as<std::expected<std::shared_ptr<scalar_file>, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<std::shared_ptr<control_file>, std::error_code> open_file(const std::filesystem::path& path,
   * control_file_open_options options) noexcept
   *
   */
  {
    t.open_file(std::declval<const std::filesystem::path&>(), std::declval<control_file_open_options>())
  } noexcept -> std::same_as<std::expected<std::shared_ptr<control_file>, std::error_code>>;
};

/**
 * @brief This concept defines the interface of a stream file which can be randomly read or write. That means you can
 * read \ write from anywhere in each read / write function calls, the offset parameter is requireds to specify the read
 * / write position in the file.
 *
 */
template <typename T>
concept is_fs_stream_file_handle = requires(const T& ct) {
  /**
   * @brief Must define a function:
   *
   *  node_info get_info() noexcept
   *
   */
  { ct.get_info() } noexcept -> std::same_as<node_info>;

  /**
   * @brief Must define a function:
   *
   *  size_t max_size() const noexcept;
   *
   */
  { ct.max_size() } noexcept -> std::same_as<size_t>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<size_t, std::error_code> read(size_t offset, mutable_buffer_sequence buffers) const noexcept;
   *
   */
  {
    ct.read(std::declval<size_t>(), std::declval<mutable_buffer_sequence>())
  } noexcept -> std::same_as<std::expected<size_t, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<size_t, std::error_code> write(size_t offset, const_buffer_sequence buffers) const noexcept;
   *
   */
  {
    ct.write(std::declval<size_t>(), std::declval<const_buffer_sequence>())
  } noexcept -> std::same_as<std::expected<size_t, std::error_code>>;
};

/**
 * @brief This concept defines the interface of a sequential stream file which means the file can't be freely read or
 * write from anywhere. You can get the read / write position by tell_xxx but whether you can change the position
 * depends on the return value of `seekable`: A seq_stream_file may or may not be seekable, it depends on the
 * implementation. Opposite to the `stream_file`, `seq_stream_file`'s read / write method don't have offset argument.
 *
 */
template <typename T>
concept is_fs_seq_stream_file_handle = requires(T t, const T& ct) {
  /**
   * @brief Must define a function:
   *
   *  node_info get_info() noexcept
   *
   */
  { ct.get_info() } noexcept -> std::same_as<node_info>;

  /**
   * @brief Must define a function:
   *
   *  size_t max_size() const noexcept;
   *
   */
  { ct.max_size() } noexcept -> std::same_as<size_t>;

  /**
   * @brief Must define a function:
   *
   *  size_t tell_read() const noexcept;
   *
   */
  { ct.tell_read() } noexcept -> std::same_as<size_t>;

  /**
   * @brief Must define a function:
   *
   *  size_t tell_write() const noexcept;
   *
   */
  { ct.tell_write() } noexcept -> std::same_as<size_t>;

  /**
   * @brief Must define a function:
   *
   *  bool seekable() const noexcept;
   *
   */
  { ct.seekable() } noexcept -> std::same_as<bool>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> seek_read(int64_t offset, seq_stream_file::seek_whence whence) noexcept;
   *
   */
  {
    t.seek_read(std::declval<int64_t>(), std::declval<seq_stream_file::seek_whence>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> seek_write(int64_t offset, seq_stream_file::seek_whence whence) noexcept;
   *
   */
  {
    t.seek_write(std::declval<int64_t>(), std::declval<seq_stream_file::seek_whence>())
  } noexcept -> std::same_as<std::expected<void, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<size_t, std::error_code> read(mutable_buffer_sequence buffers) const noexcept;
   *
   */
  { ct.read(std::declval<mutable_buffer_sequence>()) } noexcept -> std::same_as<std::expected<size_t, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<size_t, std::error_code> write(const_buffer_sequence buffers) const noexcept;
   *
   */
  { ct.write(std::declval<const_buffer_sequence>()) } noexcept -> std::same_as<std::expected<size_t, std::error_code>>;
};

/**
 * @brief This concept defines the interface of a mapped file. A mapped file is a special file that the file content is
 * directly mapped to memory, so there's no read or write method, just a `data` method to get the mapped memory address,
 * and also there will be zero copy cost - you just read from or write to the memory address directly.
 *
 */
template <typename T>
concept is_fs_mapped_file_handle = requires(T t, const T& ct) {
  /**
   * @brief Must define a function:
   *
   *  node_info get_info() noexcept
   *
   */
  { ct.get_info() } noexcept -> std::same_as<node_info>;

  /**
   * @brief Must define a function:
   *
   *  size_t max_size() const noexcept;
   *
   */
  { ct.max_size() } noexcept -> std::same_as<size_t>;

  /**
   * @brief Must define a function:
   *
   *  std::span<std::byte> data() noexcept;
   *
   */
  { t.data() } noexcept -> std::same_as<std::span<std::byte>>;

  /**
   * @brief Must define a function:
   *
   *  std::span<const std::byte> data() const noexcept;
   *
   */
  { ct.data() } noexcept -> std::same_as<std::span<const std::byte>>;

  /**
   * @brief Must define a function:
   *
   *  size_t reservation() const noexcept;
   *
   */
  { ct.reservation() } noexcept -> std::same_as<size_t>;

  /**
   * @brief Must define a function:
   *
   *  size_t starting_offset() const noexcept;
   *
   */
  { ct.starting_offset() } noexcept -> std::same_as<size_t>;
};

/**
 * @brief This concept defines the interface of a scalar file which read or write a scalar value directly via std::any.
 * This kind of file can transparently operate on value without encoding or decoding, and it will improve the
 * performance in some use cases. NOTE: The value type of a scalar file must specified when creating a scalar file
 * (actually, the initial value is specified when creating a scalar file and so, the type of the scalar file is also
 * specified by the way.) and the type can't be changed unless you remove the file and create again.
 *
 */
template <typename T>
concept is_fs_scalar_file_handle = requires(T t, const T& ct) {
  /**
   * @brief Must define a function:
   *
   *  node_info get_info() noexcept
   *
   */
  { ct.get_info() } noexcept -> std::same_as<node_info>;

  /**
   * @brief Must define a function:
   *
   *  const std::type_info& type() const noexcept;
   *
   */
  { ct.type() } noexcept -> std::same_as<const std::type_info&>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<std::any, std::error_code> read() const noexcept;
   *
   */
  { ct.read() } noexcept -> std::same_as<std::expected<std::any, std::error_code>>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> write(std::any value) noexcept;
   *
   */
  { t.write(std::declval<std::any>()) } noexcept -> std::same_as<std::expected<void, std::error_code>>;
};

/**
 * @brief This concept defines the interface of a control file which can run command by execute function. You can't
 * create a control file in most file system implementation.
 *
 */
template <typename T>
concept is_fs_control_file_handle = requires(T t, const T& ct) {
  /**
   * @brief Must define a function:
   *
   *  node_info get_info() noexcept
   *
   */
  { ct.get_info() } noexcept -> std::same_as<node_info>;

  /**
   * @brief Must define a function:
   *
   *  std::expected<void, std::error_code> execute(std::string_view command) noexcept;
   *
   */
  { t.execute(std::declval<std::string_view>()) } noexcept -> std::same_as<std::expected<void, std::error_code>>;
};

} // namespace detail
} // namespace vfs9
