/**
 * @file vns.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Virtual namespace
 * @version 0.1
 * @date 2026-09-28
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <boost/describe.hpp>

#include "vfs9/buffer.hpp"
#include "vfs9/detail/lock.hpp"
#include "vfs9/detail/mount_table.hpp"
#include "vfs9/fs.hpp"
#include "vfs9/fs/mem/fs.hpp"
#include "vfs9/tracer.hpp"

namespace vfs9 {

BOOST_DEFINE_ENUM_CLASS(ns_type, standalone, shadow);

/**
 * @brief Virtual namespace
 *
 */
class vnamespace final : public std::enable_shared_from_this<vnamespace> {
  struct private_placeholder {};

public:
  /**
   * @brief Construct a new vnamespace object
   *
   */
  vnamespace() : vnamespace(memfs(), nullptr) {}

  /**
   * @brief Construct a new vnamespace object
   *
   * @param params Memory file system parameters
   * @param p_tracer Tracer object pointer
   */
  vnamespace(memfs params, std::shared_ptr<vfs9::tracer> p_tracer);

  /**
   * @brief Construct a new vnamespace object
   *
   *  NOTE: This constructor is private actually
   *
   * @param type Namespace type
   * @param p_parent Readonly parent namespace pointer
   * @param p_tracer Tracer pointer
   * @param root_nref Root node reference
   */
  vnamespace(private_placeholder,
             ns_type type,
             std::shared_ptr<const vnamespace> p_parent,
             std::shared_ptr<vfs9::tracer> p_tracer,
             detail::node_ref root_nref);

  /**
   * @brief Construct a new vnamespace object
   *
   *  NOTE: This constructor is private actually
   *
   * @param type Namespace type
   * @param p_tracer Tracer pointer
   * @param root_nref Root node reference
   * @param mt Mount table
   */
  vnamespace(private_placeholder,
             ns_type type,
             std::shared_ptr<vfs9::tracer> p_tracer,
             detail::node_ref root_nref,
             detail::mount_table mt);

  vnamespace(const vnamespace&) = delete;
  vnamespace(vnamespace&&) = delete;

  ~vnamespace() = default;

  vnamespace& operator=(const vnamespace&) = delete;
  vnamespace& operator=(vnamespace&&) = delete;

  /**
   * @brief Get namespace id
   *
   * @return uint32_t
   */
  inline uint32_t id() const noexcept { return id_; }

  /**
   * @brief Get tracer pointer, may be nullptr
   *
   * @return const vfs9::tracer*
   */
  inline const vfs9::tracer* tracer() const noexcept { return p_tracer_.get(); }

  // ------ Copy apis
  /**
   * @brief Clone this namespace, will return a new standalone namespace
   *
   * @return std::shared_ptr<vnamespace>
   */
  std::shared_ptr<vnamespace> clone() const;

  /**
   * @brief Shadow this namespace, will return a new shadow namespace
   *
   *  NOTE: You can only shadow a standalone namespace.
   *
   * @return std::shared_ptr<vnamespace>
   */
  std::expected<std::shared_ptr<vnamespace>, std::error_code> shadow() const;

  // ------ Mount / bind / freeze apis
  /**
   * @brief Mount a file system
   *
   *  Mount a file system to a path, see comments of `bind`
   *
   * @tparam T File system (params) type
   * @param path Path to mount
   * @param mflag Merge flag
   * @param writable Writable or not
   * @param fs_params File system params
   * @return std::error_code
   */
  template <typename T, typename... Ts>
    requires(detail::is_fs<std::remove_cvref_t<T>>)
  std::expected<void, std::error_code>
  mount(std::filesystem::path path, merge_flag mflag, bool writable, T&& fs_params, Ts&&... fs_args) {
    return mount(std::move(path), mflag, writable, {}, std::forward<T>(fs_params), std::forward<Ts>(fs_args)...);
  }

  /**
   * @brief Mount a file system
   *
   *  Mount a file system to a path, see comments of `bind`
   *
   * @tparam T File system (params) type
   * @param path Path to mount
   * @param mflag Merge flag
   * @param writable Writable or not
   * @param name Mount name
   * @param fs_params File system params
   * @return std::error_code
   */
  template <typename T, typename... Ts>
    requires(detail::is_fs<std::remove_cvref_t<T>>)
  std::expected<void, std::error_code> mount(std::filesystem::path path,
                                             merge_flag mflag,
                                             bool writable,
                                             std::string_view name,
                                             T&& fs_params,
                                             Ts&&... fs_args) {
    // NOTE: The lock & freeze check are performed inside the type-erased mount implementation.
    return mount(
        std::move(path),
        [&]() -> std::expected<std::shared_ptr<detail::file_system>, std::error_code> {
          return detail::make_file_system(std::forward<T>(fs_params), std::forward<Ts>(fs_args)...);
        },
        mflag,
        writable,
        name);
  }

  /**
   * @brief Bind new path to old path
   *
   *  This method will replace / merge the old path with the node which is targeted by new path. The merge behavior is
   * defined by merge flag:
   *  - replace New node will replace the node that old path is targeting
   *  - before A union directory will be created if old path isn't targeting to a union directory and new node will be
   * added in the front of old node. If old path is already targeting a union directory, this method will add new node
   * in front of all other nodes in union directory
   *  - after The same as before except the new node will be added at the end of union directory
   *
   * @param new_path New path
   * @param old_path old path
   * @param mflag Merge flag
   * @param writable Writable or not
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code>
  bind(std::filesystem::path new_path, std::filesystem::path old_path, merge_flag mflag, bool writable);

  /**
   * @brief Unmount a file system from old path
   *
   * @param name Name
   * @param old_path Old path
   * @return bool unmounted or not
   */
  std::expected<bool, std::error_code> unmount(std::string_view name, std::filesystem::path old_path) noexcept;

  /**
   * @brief Unmount a path from old path
   *
   * @param new_path New path
   * @param old_path Old path
   * @return bool unmounted or not
   */
  std::expected<bool, std::error_code> unmount(std::filesystem::path new_path, std::filesystem::path old_path) noexcept;

  /**
   * @brief Unmount all mount / bind rules from old path
   *
   * @param old_path Old path
   * @return bool unmounted or not
   */
  std::expected<bool, std::error_code> unmount(std::filesystem::path old_path) noexcept;

  /**
   * @brief Unmount all mount / bind rules from all path
   *
   */
  std::expected<void, std::error_code> unmount() noexcept;

  /**
   * @brief Freeze the namespace
   *
   */
  inline void freeze() noexcept { mutex_.freeze(); }

  /**
   * @brief Is the namespace frozen
   *
   * @return true
   * @return false
   */
  inline bool is_frozen() const noexcept { return mutex_.is_frozen(); }

  // ------ File system apis

  /**
   * @brief Get the status of a path
   *
   * @param path Path to query
   * @return std::expected<node_status, std::error_code> node_status::not_found if the path doesn't exist
   */
  std::expected<node_status, std::error_code> status(std::filesystem::path path) const noexcept;

  /**
   * @brief Whether a path exists or not
   *
   * @param path Path to query
   * @return std::expected<bool, std::error_code>
   */
  std::expected<bool, std::error_code> exists(std::filesystem::path path) const noexcept;

  /**
   * @brief Whether a path exists or not
   *
   * @param path Path to query
   * @param nt Node type
   * @return std::expected<bool, std::error_code>
   */
  std::expected<bool, std::error_code> exists(std::filesystem::path path, node_type nt) const noexcept;

  /**
   * @brief Get node info of a path
   *
   * @param path Path to query
   * @return std::expected<node_info, std::error_code>
   */
  std::expected<node_info, std::error_code> get_info(std::filesystem::path path) const noexcept;

  /**
   * @brief List the entries of a directory path
   *
   *  For a union directory, entries of all its mount entries are merged: the first entry providing a
   *  given name (by priority) wins.
   *
   * @param path Path to list
   * @return std::expected<std::vector<node_entry>, std::error_code>
   */
  std::expected<std::vector<node_entry>, std::error_code> list(std::filesystem::path path) const noexcept;

  /**
   * @brief Create a directory
   *
   * @param path Directory path
   * @param recursive Recursively create directory or not, false by default
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> create_directory(std::filesystem::path path, bool recursive = false) noexcept;

  /**
   * @brief Create a placeholder file
   *
   * @param path File path
   * @param spec Placeholder file spec
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> create_file(std::filesystem::path path,
                                                   placeholder_file_create_spec spec) noexcept;

  /**
   * @brief Create a regular file
   *
   * @param path File path
   * @param spec Regular file spec
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> create_file(std::filesystem::path path, regular_file_create_spec spec) noexcept;

  /**
   * @brief Create a scalar file
   *
   * @param path File path
   * @param spec Scalar file spec
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> create_file(std::filesystem::path path, scalar_file_create_spec spec) noexcept;

  /**
   * @brief Remove a directory or file
   *
   * @param path Path to remove
   * @param recursive Recursively remove or not, must be true if you wanna remove a directory, false by default
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code> remove(std::filesystem::path path, bool recursive = false) noexcept;

  /**
   * @brief Open a stream file
   *
   * @param path File path
   * @param options Stream file open options
   * @return std::expected<std::shared_ptr<stream_file>, std::error_code>
   */
  std::expected<std::shared_ptr<stream_file>, std::error_code> open_file(std::filesystem::path path,
                                                                         stream_file_open_options options) noexcept;

  /**
   * @brief Open a sequential stream file
   *
   * @param path File path
   * @param options Sequential stream file open options
   * @return std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
   */
  std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
  open_file(std::filesystem::path path, seq_stream_file_open_options options) noexcept;

  /**
   * @brief Open a mapped file
   *
   * @param path File path
   * @param options Mapped file open options
   * @return std::expected<std::shared_ptr<mapped_file>, std::error_code>
   */
  std::expected<std::shared_ptr<mapped_file>, std::error_code> open_file(std::filesystem::path path,
                                                                         mapped_file_open_options options) noexcept;

  /**
   * @brief Open a scalar file
   *
   * @param path File path
   * @param options Scalar file open options
   * @return std::expected<std::shared_ptr<scalar_file>, std::error_code>
   */
  std::expected<std::shared_ptr<scalar_file>, std::error_code> open_file(std::filesystem::path path,
                                                                         scalar_file_open_options options) noexcept;

  /**
   * @brief Open a control file
   *
   * @param path File path
   * @param options Control file open options
   * @return std::expected<std::shared_ptr<control_file>, std::error_code>
   */
  std::expected<std::shared_ptr<control_file>, std::error_code> open_file(std::filesystem::path path,
                                                                          control_file_open_options options) noexcept;

  /**
   * @brief Utility function, read all file data to buffers
   *
   * @param path File path
   * @param buffers Buffers
   * @param options Sequential stream file open options
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code>
  read_all(std::filesystem::path path, mutable_buffer_sequence buffers, seq_stream_file_open_options options) noexcept;

  /**
   * @brief Utility function, write all buffers data to file
   *
   * @param path File path
   * @param buffers Buffers
   * @param options Sequential stream file open options
   * @return std::expected<size_t, std::error_code>
   */
  std::expected<size_t, std::error_code>
  write_all(std::filesystem::path path, const_buffer_sequence buffers, seq_stream_file_open_options options) noexcept;

  /**
   * @brief Utility function, read scalar value from file
   *
   * @param path File path
   * @param options Scalar file open options
   * @return std::expected<std::any, std::error_code>
   */
  std::expected<std::any, std::error_code> read_scalar(std::filesystem::path path,
                                                       scalar_file_open_options options) noexcept;

  /**
   * @brief Utility function, write scalar value to file
   *
   * @param path File path
   * @param value Value to write
   * @param options Scalar file open options
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code>
  write_scalar(std::filesystem::path path, std::any value, scalar_file_open_options options) noexcept;

  /**
   * @brief Utility function, execute a command by a control file
   *
   * @param path File path
   * @param command Command to execute
   * @param options Control file open options
   * @return std::expected<void, std::error_code>
   */
  std::expected<void, std::error_code>
  execute(std::filesystem::path path, std::string_view command, control_file_open_options options) noexcept;

private:
  friend class shared_ns_lock;
  friend class unique_ns_lock;

  /**
   * @brief Lookup the longest mount point
   *
   *  DANGER: Must be protected by read-lock
   *
   * @param ref
   * @param begin
   * @param end
   * @return std::tuple<detail::lookup_mount_point_result, std::filesystem::path::const_iterator>
   */
  std::tuple<detail::lookup_mount_point_result, std::filesystem::path::const_iterator>
  lookup_longest_mount_point(detail::node_ref_view nref,
                             std::filesystem::path::const_iterator begin,
                             std::filesystem::path::const_iterator end) const noexcept;

  /**
   * @brief The result of lookup_path
   *
   */
  class lookup_path_result;

  /**
   * @brief Lookup a path
   *
   *  NOTE: This method doesn't guarantee that the returned paths are all existed.
   *  DANGER: Must be protected by read-lock
   *
   * @param path Path to look up
   * @return lookup_path_result
   */
  lookup_path_result lookup_path(const std::filesystem::path& path) const noexcept;

  /**
   * @brief Lookup a path
   *
   *  NOTE: This method doesn't guarantee that the returned paths are all existed.
   *  DANGER: Must be protected by read-lock
   *
   * @param fs File system
   * @param begin Path begin
   * @param end Path end
   * @return lookup_path_result
   */
  lookup_path_result lookup_path(detail::node_ref_view ref,
                                 std::filesystem::path::const_iterator begin,
                                 std::filesystem::path::const_iterator end) const noexcept;

  /**
   * @brief Lookup a path
   *
   *  NOTE: This method doesn't guarantee that the returned paths are all existed.
   *  DANGER: Must be protected by read-lock
   *
   * @param mp_result
   * @param begin
   * @param end
   * @return lookup_path_result
   */
  lookup_path_result lookup_path(const detail::lookup_mount_point_result& mp_result,
                                 std::filesystem::path::const_iterator begin,
                                 std::filesystem::path::const_iterator end) const noexcept;

  /**
   * @brief Get a mutable mount point which is safe to modify
   *
   *  If the mount point is inherited from the parent namespace (shadow), it is first copied into the
   *  local mount table so that modifying it won't corrupt the parent namespace.
   *
   *  DANGER: Must be protected by write-lock
   *
   * @param path_result Path lookup result which must be targeting a mount point
   * @param p_from_parent Set to true if the mount point was inherited from the parent namespace
   * @return detail::mount_point* Mutable mount point, or nullptr if the result isn't a mount point
   */
  detail::mount_point* mutable_mount_point(const lookup_path_result& path_result, bool* p_from_parent = nullptr);

  /**
   * @brief Resolve the target node reference and the file system path to create for a creation operation
   *
   *  If `recursive` is false the direct parent of `path` must exist; if it is true the lookup walks up
   *  to the nearest existing directory and lets the file system create the missing components.
   *
   *  DANGER: Must be protected by read-lock
   *
   * @param path Path to create
   * @param recursive Whether missing intermediate directories may be created
   * @return std::expected<std::pair<detail::node_ref, std::filesystem::path>, std::error_code>
   */
  std::expected<std::pair<detail::node_ref, std::filesystem::path>, std::error_code>
  resolve_creation_target(const std::filesystem::path& path, bool recursive) const noexcept;

  using new_fs_func_type = std::function<std::expected<std::shared_ptr<detail::file_system>, std::error_code>(void)>;

  /**
   * @brief Mount a file system
   *
   *  DANGER: Must be protected by write-lock
   *
   * @param path Path to mount file system
   * @param new_fs_func Function to create new file system
   * @param mflag Merge flag
   * @param writable Writable or not
   * @param name Optional mount name
   */
  std::expected<void, std::error_code> mount(std::filesystem::path path,
                                             const new_fs_func_type& new_fs_func,
                                             merge_flag mflag,
                                             bool writable,
                                             std::string_view name = {});

  /**
   * @brief Mount a node to old path result
   *
   *  DANGER: Must be protected by write-lock
   *
   * @param path_result
   * @param nref
   * @param mflag
   * @param writable
   * @param name Optional name
   */
  void mount(const lookup_path_result& path_result,
             detail::node_ref nref,
             merge_flag mflag,
             bool writable,
             std::string_view name = {});

  /**
   * @brief Mount a mount point to old path result
   *
   *  DANGER: Must be protected by write-lock
   *
   * @param path_result
   * @param mp
   * @param mflag
   * @param writable
   * @param name Optional name
   */
  void mount(const lookup_path_result& path_result,
             const detail::mount_point& mp,
             merge_flag mflag,
             bool writable,
             std::string_view name = {});

  uint32_t id_;                                  // Auto incremental id
  ns_type type_;                                 // Namespace type
  std::shared_ptr<const vnamespace> p_parent_;   // Guarantee the parent namespace is alive
  const std::shared_ptr<vfs9::tracer> p_tracer_; // Pointer can't be changed after constructed
  detail::node_ref root_nref_;                   // Root file system node
  detail::mount_table mount_table_;              // Local mount table
  detail::freezable_mutex mutex_;

  // Global namespace id (ns_id)
  static std::atomic<uint32_t> next_id_;
};

/**
 * @brief Create a new vnamespace object
 *
 * @return std::shared_ptr<vnamespace>
 */
inline std::shared_ptr<vnamespace> make_namespace() { return std::make_shared<vnamespace>(); }

/**
 * @brief Create a new vnamespace object
 *
 * @param params Memory file system parameters
 * @param tracer Tracer shared pointer, nullptr by default
 * @return std::shared_ptr<vnamespace>
 */
inline std::shared_ptr<vnamespace> make_namespace(memfs params, std::shared_ptr<vfs9::tracer> tracer = nullptr) {
  return std::make_shared<vnamespace>(std::move(params), std::move(tracer));
}

} // namespace vfs9
