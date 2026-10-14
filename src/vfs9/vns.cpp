/**
 * @file vns.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Virtual namespace
 * @version 0.1
 * @date 2026-09-28
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/vns.hpp"

#include <ranges>
#include <stdexcept>
#include <string>

#include "vfs9/detail/utility.hpp"
#include "vfs9/error.hpp"

namespace vfs9 {

/**
 * @brief Global namespace id
 *
 */
std::atomic<uint32_t> vnamespace::next_id_ = 0;

// ------ path

/**
 * @brief Normalize the path in place, this function does the following things:
 *
 *  1. Guarantee the path is an absolute path
 *  2. Path is lexically normalized
 *  3. The tailing back slash will be removed if exists
 *
 * @param path Path to normalize, NOTE: the path object will be modified since this is a in-place normalize function
 * @return std::error_code
 */
inline std::error_code normalize_path_in_place(std::filesystem::path& path) noexcept {
  if (!path.is_absolute()) {
    return make_error_code(errc::not_an_abs_path);
  }
  path = path.lexically_normal();
  if (!path.has_filename() && path != path.root_path()) { // Remove the tailing back slash
    path = path.parent_path();
  }
  return {};
}

// ------ lock

/**
 * @brief Shared namespace lock which supports lock self & parent at the same time
 *
 */
class shared_ns_lock {
public:
  explicit shared_ns_lock(const vnamespace* p_ns) : self_lk_(p_ns->mutex_) {
    if (p_ns->p_parent_) {
      parent_lk_.emplace(p_ns->p_parent_->mutex_);
    }
  }

  ~shared_ns_lock() = default;

private:
  detail::shared_freezable_lock self_lk_;
  std::optional<detail::shared_freezable_lock> parent_lk_;
};

/**
 * @brief Unique namespace lock which supports lock self & parent at the same time
 *
 *  NOTE: Parent lock is always shared, that means parent is always readonly.
 *
 */
class unique_ns_lock {
public:
  explicit unique_ns_lock(const vnamespace* p_ns) : self_lk_(p_ns->mutex_) {
    if (!self_lk_.error() && p_ns->p_parent_) {
      parent_lk_.emplace(p_ns->p_parent_->mutex_);
    }
  }

  ~unique_ns_lock() = default;

  // Get error
  inline const std::error_code& error() const noexcept { return self_lk_.error(); }

  // Get movable error
  inline std::error_code&& error() noexcept { return self_lk_.error(); }

private:
  detail::unique_freezable_lock self_lk_;
  // Parent lock is always shared, that means parent is always readonly.
  std::optional<detail::shared_freezable_lock> parent_lk_;
};

// ------ vnamespace

vnamespace::vnamespace(memfs params, std::shared_ptr<vfs9::tracer> p_tracer)
    : id_(++next_id_), type_(ns_type::standalone), p_parent_(nullptr), p_tracer_(std::move(p_tracer)) {
  // Create root memfs file system
  auto fs_result = detail::make_file_system(params);
  if (!fs_result) {
    // A root memfs can only fail to initialize if the allocator throws, so this is unrecoverable.
    throw std::runtime_error("Failed to create the root memfs: " + fs_result.error().message());
  }
  root_nref_ = detail::node_ref(std::move(fs_result.value()), "/");
}

vnamespace::vnamespace(private_placeholder,
                       ns_type type,
                       std::shared_ptr<const vnamespace> p_parent,
                       std::shared_ptr<vfs9::tracer> p_tracer,
                       detail::node_ref root_nref)
    : id_(++next_id_)
    , type_(type)
    , p_parent_(std::move(p_parent))
    , p_tracer_(std::move(p_tracer))
    , root_nref_(root_nref)
    , mount_table_(type == ns_type::shadow && p_parent_ ? &p_parent_->mount_table_ : nullptr) {}

vnamespace::vnamespace(private_placeholder,
                       ns_type type,
                       std::shared_ptr<vfs9::tracer> p_tracer,
                       detail::node_ref root_nref,
                       detail::mount_table mt)
    : id_(++next_id_)
    , type_(type)
    , p_parent_(nullptr)
    , p_tracer_(std::move(p_tracer))
    , root_nref_(root_nref)
    , mount_table_(std::move(mt)) {}

// ------ lookup path result

/**
 * @brief Lookup path result
 *
 */
class vnamespace::lookup_path_result {
public:
  lookup_path_result() = default;

  template <typename T>
  explicit lookup_path_result(T&& value) : value_(std::forward<T>(value)) {}

  template <typename T>
  explicit lookup_path_result(T&& value, bool writable)
      : value_(std::forward<T>(value)), writable_(writable) {}

  lookup_path_result(const lookup_path_result&) = default;

  lookup_path_result(lookup_path_result&& other)
      : value_(std::move(other.value_)), writable_(other.writable_) {
    other.value_ = std::monostate{};
  }

  ~lookup_path_result() = default;

  lookup_path_result& operator=(const lookup_path_result&) = default;

  inline lookup_path_result& operator=(lookup_path_result&& other) {
    value_ = std::move(other.value_);
    writable_ = other.writable_;
    other.value_ = std::monostate{};
    return *this;
  }

  // Whether the resolved node (when the result is a node reference) is writable
  inline bool writable() const noexcept { return writable_; }

  inline operator bool() const noexcept { return !empty(); }

  // Whether the result is empty or not, empty means doesn't contain either mount point or node ref
  inline bool empty() const noexcept {
    return value_.index() == 0 || (value_.index() == 1 && std::get<1>(value_).empty());
  }

  // Get node id
  inline detail::node_id_view nid() const noexcept {
    return std::visit(
        [&](auto&& v) -> detail::node_id_view {
          using value_type = std::remove_cvref_t<decltype(v)>;
          if constexpr (std::is_same_v<value_type, std::monostate>) {
            return {};
          } else {
            return v.nid();
          }
        },
        value_);
  }

  // Whether the result is a mount point or not
  inline bool is_mount_point() const noexcept { return value_.index() == 1; }

  // Get the mount point
  inline const detail::mount_point* mount_point() const noexcept {
    if (value_.index() == 1) {
      return std::get<1>(value_).mp();
    }
    return nullptr;
  }

  // Whether the result mount point is from parent or not
  inline bool is_mount_point_from_parent() const noexcept {
    return value_.index() == 1 && std::get<1>(value_).from_parent();
  }

  /**
   * @brief Resolve the node reference which a file system operation should be applied to
   *
   * @param for_write Whether the operation writes; if true a writable mount entry is required when
   * this result is a mount point
   * @return std::expected<detail::node_ref, std::error_code>
   */
  std::expected<detail::node_ref, std::error_code> resolve_node_ref(bool for_write) const noexcept {
    if (empty()) {
      return std::unexpected(make_error_code(errc::path_not_found));
    }

    if (is_mount_point()) {
      const detail::mount_point* p_mp = mount_point();
      const detail::mount_entry* p_entry = nullptr;
      if (for_write) {
        // Pick the first writable entry (by priority); if none of the mounted entries are writable we
        // can't dispatch the write.
        p_entry = p_mp->first_writable_entry();
        if (p_entry == nullptr) {
          return std::unexpected(make_error_code(errc::unsupported));
        }
      } else {
        if (p_mp->entry_size() == 0) {
          return std::unexpected(make_error_code(errc::path_not_found));
        }
        // Pick the highest priority entry
        p_entry = &p_mp->entry_list()[0];
      }
      return detail::node_ref(p_entry->nref());
    }

    if (for_write && !writable()) {
      // The resolved node lives in a mount entry which was mounted / bound as read-only
      return std::unexpected(make_error_code(errc::unsupported));
    }
    return detail::node_ref(nref());
  }

  // Get the node ref (view), will return an empty node ref if the result is empty or storing a mount point
  inline detail::node_ref_view nref() const noexcept {
    return std::visit(
        [](auto&& v) -> detail::node_ref_view {
          using value_type = std::remove_cvref_t<decltype(v)>;
          if constexpr (std::is_same_v<value_type, std::monostate> ||
                        std::is_same_v<value_type, detail::lookup_mount_point_result>) {
            return {};
          } else {
            return detail::node_ref_view(v);
          }
        },
        value_);
  }

  // The result of check_path
  enum class path_check_result {
    not_found,
    file,
    dir,
    union_dir,
    unsupported,
  };

  // Check path
  path_check_result check_path() const noexcept {
    if (empty()) {
      return path_check_result::not_found;
    }
    if (is_mount_point() && mount_point()->entry_size() > 1) {
      return path_check_result::union_dir;
    }
    // Check node status
    node_status s;

    if (is_mount_point() && mount_point()->entry_size() == 1) {
      const auto& nref = mount_point()->entry_list()[0].nref();
      s = nref.fs()->status(nref.path());
    } else {
      s = nref().fs()->status(nref().path());
    }

    switch (s) {
    case node_status::not_found:
      return path_check_result::not_found;
    case node_status::directory:
      return path_check_result::dir;
    case node_status::file:
      return path_check_result::file;
    default:
      return path_check_result::unsupported;
    }
  }

private:
  std::variant<std::monostate, detail::lookup_mount_point_result, detail::node_ref, detail::node_ref_view> value_;
  bool writable_ = true;
};

// ------ clone / shadow

std::shared_ptr<vnamespace> vnamespace::clone() const {
  shared_ns_lock lk(this);

  // A clone is always standalone, so the inherited (parent) mount entries must be materialized into a
  // local, parentless mount table.
  return std::make_shared<vnamespace>(
      private_placeholder{}, ns_type::standalone, p_tracer_, root_nref_, mount_table_.flatten());
}

std::expected<std::shared_ptr<vnamespace>, std::error_code> vnamespace::shadow() const {
  if (type_ != ns_type::standalone) {
    return std::unexpected(make_error_code(errc::must_be_standalone_ns));
  }
  shared_ns_lock lk(this);
  return std::make_shared<vnamespace>(
      private_placeholder{}, ns_type::shadow, shared_from_this(), p_tracer_, root_nref_);
}

// ------ lookup

// Private, must be protected by read lock
std::tuple<detail::lookup_mount_point_result, std::filesystem::path::const_iterator>
vnamespace::lookup_longest_mount_point(detail::node_ref_view nref,
                                       std::filesystem::path::const_iterator begin,
                                       std::filesystem::path::const_iterator end) const noexcept {
  // Lookup mount table
  auto [mp_result, next_it] = mount_table_.lookup_nearest(nref.fs()->id(), nref.path(), begin, end);
  if (mp_result) {
    // No more path to lookup
    if (next_it == end) {
      return std::make_tuple(std::move(mp_result), std::move(end));
    }
    // Continuously lookup path by checking each entry in the mount point
    for (const auto& entry : mp_result.mp()->entry_list()) {
      if (entry.nref().fs()->exists(entry.nref().path(), next_it->c_str(), node_type::directory)) {
        // Found the first entry which contains the sub directory. Try to find a deeper mount point
        // inside that entry; if there is none, keep this (nearest) mount point so that the remainder
        // of the path is resolved inside the mounted file system.
        auto deeper = lookup_longest_mount_point(entry.nref(), next_it, end);
        if (std::get<0>(deeper)) {
          return deeper;
        }
        break;
      }
    }
    return std::make_tuple(std::move(mp_result), std::move(next_it));
  }
  // Not found
  return std::make_tuple(detail::lookup_mount_point_result(), std::move(begin));
}

// Private, must be protected by read lock
vnamespace::lookup_path_result vnamespace::lookup_path(const std::filesystem::path& path) const noexcept {
  if (path.empty()) {
    return {};
  }
  // Lookup mount table
  auto [mp_result, next_it] = lookup_longest_mount_point(root_nref_, path.begin(), path.end());
  // No mount point is found, return root file system
  if (!mp_result) {
    return lookup_path_result(detail::node_ref_view(root_nref_.fs(), &path));
  }
  if (next_it == path.end()) {
    return lookup_path_result(std::move(mp_result));
  }
  // Lookup path from mount point
  return lookup_path(mp_result, next_it, path.end());
}

// Private, must be protected by read lock
vnamespace::lookup_path_result vnamespace::lookup_path(detail::node_ref_view nref,
                                                       std::filesystem::path::const_iterator begin,
                                                       std::filesystem::path::const_iterator end) const noexcept {
  if (begin == end) {
    return {};
  }
  // Lookup mount point
  auto [mp_result, next_it] = lookup_longest_mount_point(nref, begin, end);
  // No mount point is found, use current node ref
  if (!mp_result) {
    return lookup_path_result(detail::node_ref(nref.fs(), detail::make_path_from_range(nref.path(), begin, end)));
  }
  if (next_it == end) {
    return lookup_path_result(std::move(mp_result));
  }
  // Lookup path from mount point
  return lookup_path(mp_result, next_it, std::move(end));
}

// Private, must be protected by read lock
vnamespace::lookup_path_result vnamespace::lookup_path(const detail::lookup_mount_point_result& mp_result,
                                                       std::filesystem::path::const_iterator begin,
                                                       std::filesystem::path::const_iterator end) const noexcept {
  if (!mp_result) {
    return lookup_path_result();
  }
  if (begin == end) {
    return lookup_path_result(std::move(mp_result));
  }
  // Choose one entry in the mount point. NOTE: The remaining component may be a file (the leaf), so
  // accept any node type here; the file system status of the full path decides file/directory/not_found.
  for (const auto& entry : mp_result.mp()->entry_list()) {
    if (entry.nref().fs()->exists(entry.nref().path(), begin->c_str())) {
      // Found the first entry which contains the remaining path. Carry the entry's writability so
      // that write operations dispatched into this entry can be gated.
      return lookup_path_result(
          detail::node_ref(entry.nref().fs(), detail::make_path_from_range(entry.nref().path(), begin, end)),
          entry.writable());
    }
  }
  return lookup_path_result();
}

// ------ bind / mount / unmount

std::expected<void, std::error_code>
vnamespace::bind(std::filesystem::path new_path, std::filesystem::path old_path, merge_flag mflag, bool writable) {
  // Normalize new & old path
  auto ec = normalize_path_in_place(new_path);
  if (ec) {
    return std::unexpected(std::move(ec));
  }

  ec = normalize_path_in_place(old_path);
  if (ec) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  // Lookup new & old path
  auto new_path_result = lookup_path(new_path);
  if (!new_path_result) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }

  auto old_path_result = lookup_path(old_path);
  if (!old_path_result) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }

  auto new_path_check_result = new_path_result.check_path();
  auto old_path_check_result = old_path_result.check_path();

  if (new_path_check_result == lookup_path_result::path_check_result::not_found ||
      old_path_check_result == lookup_path_result::path_check_result::not_found) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  if (new_path_check_result == lookup_path_result::path_check_result::unsupported ||
      old_path_check_result == lookup_path_result::path_check_result::unsupported) {
    return std::unexpected(make_error_code(errc::unsupported));
  }

  bool new_path_is_dir_or_union_dir = new_path_check_result == lookup_path_result::path_check_result::dir ||
                                      new_path_check_result == lookup_path_result::path_check_result::union_dir;
  bool old_path_is_dir_or_union_dir = old_path_check_result == lookup_path_result::path_check_result::dir ||
                                      old_path_check_result == lookup_path_result::path_check_result::union_dir;

  if (new_path_is_dir_or_union_dir != old_path_is_dir_or_union_dir) {
    return std::unexpected(make_error_code(errc::different_node_type));
  }

  if (mflag != merge_flag::replace && (!new_path_is_dir_or_union_dir || !old_path_is_dir_or_union_dir)) {
    return std::unexpected(make_error_code(errc::not_a_dir_or_union_dir));
  }

  // NOTE: The `name` parameter is normalized new_path
  if (new_path_result.is_mount_point()) {
    mount(old_path_result, *new_path_result.mount_point(), mflag, writable, new_path.c_str());
  } else {
    mount(old_path_result, new_path_result.nref(), mflag, writable, new_path.c_str());
  }
  return {};
}

// Private, must be protected by write lock
std::expected<void, std::error_code> vnamespace::mount(std::filesystem::path path,
                                                       const new_fs_func_type& new_fs_func,
                                                       merge_flag mflag,
                                                       bool writable,
                                                       std::string_view name) {
  // Normalize path
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  // Lookup path
  auto path_result = lookup_path(path);
  if (!path_result) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  auto path_check_status = path_result.check_path();
  if (path_check_status == lookup_path_result::path_check_result::not_found) {
    return std::unexpected(make_error_code(errc::path_not_found));
  }
  if (path_check_status != lookup_path_result::path_check_result::dir &&
      path_check_status != lookup_path_result::path_check_result::union_dir) {
    return std::unexpected(make_error_code(errc::not_a_dir_or_union_dir));
  }

  // Create new file system
  auto new_fs_result = new_fs_func();
  if (!new_fs_result) {
    return std::unexpected(new_fs_result.error());
  }
  auto p_fs = std::move(new_fs_result.value());

  // Mount
  mount(path_result, detail::node_ref(p_fs, "/"), mflag, writable, name);
  return {};
}

// Private, must be protected by write lock
void vnamespace::mount(const lookup_path_result& path_result,
                       detail::node_ref nref,
                       merge_flag mflag,
                       bool writable,
                       std::string_view name) {
  if (mflag == merge_flag::replace) {
    mount_table_.assign(path_result.nid(), std::move(nref), writable, name);
    return;
  }

  // Old path is targeting a mount point. NOTE: `mutable_mount_point` materializes a mount point
  // inherited from the parent (shadow) into the local mount table before it can be modified.
  if (path_result.is_mount_point()) {
    mutable_mount_point(path_result)->add(mflag == merge_flag::before, std::move(nref), writable, name);
    return;
  }

  // Old path is targeting a node reference. NOTE: The original node is writable by default (see README).
  if (mflag == merge_flag::before) {
    mount_table_.assign(path_result.nid(),
                        detail::mount_point({
                            detail::mount_entry(std::move(nref), writable, name),
                            detail::mount_entry(path_result.nref(), true),
                        }));
  } else {
    mount_table_.assign(path_result.nid(),
                        detail::mount_point({
                            detail::mount_entry(path_result.nref(), true),
                            detail::mount_entry(std::move(nref), writable, name),
                        }));
  }
}

// Private, must be protected by write lock
void vnamespace::mount(const lookup_path_result& path_result,
                       const detail::mount_point& mp,
                       merge_flag mflag,
                       bool writable,
                       std::string_view name) {

  if (mp.empty()) {
    return;
  }

  // Mount by node ref
  if (mp.entry_size() == 1) {
    mount(path_result, mp[0].nref(), mflag, writable, name);
    return;
  }

  // Copied entries, created by new writable & name parameters
  auto copied_entries = std::views::transform(mp.entry_list(), [&](const detail::mount_entry& entry) {
    return detail::mount_entry(entry.nref(), writable, name);
  });

  if (mflag == merge_flag::replace) {
    mount_table_.assign(path_result.nid(), copied_entries.begin(), copied_entries.end());
    return;
  }

  // Old path is targeting a mount point. NOTE: `mutable_mount_point` materializes a mount point
  // inherited from the parent (shadow) into the local mount table before it can be modified.
  if (path_result.is_mount_point()) {
    mutable_mount_point(path_result)
        ->add(mflag == merge_flag::before, copied_entries.begin(), copied_entries.end());
    return;
  }

  // Old path is targeting a node reference. NOTE: The original node is writable by default (see README).
  mount_table_.assign(path_result.nid(), copied_entries.begin(), copied_entries.end())
      .add(mflag != merge_flag::before, path_result.nref(), true);
}

// Private, must be protected by write lock
detail::mount_point* vnamespace::mutable_mount_point(const lookup_path_result& path_result, bool* p_from_parent) {
  if (!path_result.is_mount_point()) {
    return nullptr;
  }
  const bool from_parent = path_result.is_mount_point_from_parent();
  if (p_from_parent) {
    *p_from_parent = from_parent;
  }
  if (from_parent) {
    // Materialize the inherited mount point into the local mount table before modifying it
    detail::mount_point copy(*path_result.mount_point());
    return &mount_table_.assign(path_result.nid(), std::move(copy));
  }
  return const_cast<detail::mount_point*>(path_result.mount_point());
}

// Private, must be protected by read lock
std::expected<std::pair<detail::node_ref, std::filesystem::path>, std::error_code>
vnamespace::resolve_creation_target(const std::filesystem::path& path, bool recursive) const noexcept {
  const std::filesystem::path name = path.filename();
  if (name.empty()) {
    return std::unexpected(make_error_code(errc::invalid_name));
  }

  // Walk up from the direct parent until an existing directory (or union directory) is found.
  std::filesystem::path current = path.parent_path();
  for (;;) {
    auto result = lookup_path(current);
    auto check = result.check_path();
    if (check == lookup_path_result::path_check_result::dir ||
        check == lookup_path_result::path_check_result::union_dir) {
      auto target = result.resolve_node_ref(true);
      if (!target) {
        return std::unexpected(target.error());
      }
      std::filesystem::path remaining = path.lexically_relative(current);
      if (remaining.empty()) {
        remaining = name;
      }
      std::filesystem::path fs_path = target.value().path() / remaining;
      return std::make_pair(std::move(target.value()), std::move(fs_path));
    }
    if (check == lookup_path_result::path_check_result::file) {
      return std::unexpected(make_error_code(errc::not_a_dir));
    }
    if (!recursive) {
      return std::unexpected(make_error_code(errc::path_not_found));
    }
    if (current == current.root_path()) {
      break;
    }
    current = current.parent_path();
  }
  return std::unexpected(make_error_code(errc::path_not_found));
}

std::expected<bool, std::error_code> vnamespace::unmount(std::string_view name,
                                                         std::filesystem::path old_path) noexcept {
  if (auto ec = normalize_path_in_place(old_path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto path_result = lookup_path(old_path);
  // Path not found or not a mount point
  if (!path_result || !path_result.is_mount_point()) {
    return false;
  }

  bool from_parent = false;
  auto* p_mp = mutable_mount_point(path_result, &from_parent);
  if (p_mp == nullptr || !p_mp->remove(name)) {
    // Roll back the copy materialized from the parent, the parent is left untouched.
    if (from_parent) {
      mount_table_.remove(path_result.nid());
    }
    return false;
  }

  // Remove the entire mount point
  if (p_mp->entry_size() == 0) {
    mount_table_.remove(path_result.nid());
    if (from_parent) {
      // An inherited mount point can only be masked, not entirely removed
      return std::unexpected(make_error_code(errc::unsupported));
    }
  }
  return true;
}

std::expected<bool, std::error_code> vnamespace::unmount(std::filesystem::path new_path,
                                                         std::filesystem::path old_path) noexcept {
  if (auto ec = normalize_path_in_place(new_path)) {
    return std::unexpected(std::move(ec));
  }
  if (auto ec = normalize_path_in_place(old_path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto path_result = lookup_path(old_path);
  // Path not found or not a mount point
  if (!path_result || !path_result.is_mount_point()) {
    return false;
  }

  bool from_parent = false;
  auto* p_mp = mutable_mount_point(path_result, &from_parent);
  if (p_mp == nullptr || !p_mp->remove(new_path.c_str())) {
    // Roll back the copy materialized from the parent, the parent is left untouched.
    if (from_parent) {
      mount_table_.remove(path_result.nid());
    }
    return false;
  }

  // Remove the entire mount point
  if (p_mp->entry_size() == 0) {
    mount_table_.remove(path_result.nid());
    if (from_parent) {
      // An inherited mount point can only be masked, not entirely removed
      return std::unexpected(make_error_code(errc::unsupported));
    }
  }
  return true;
}

std::expected<bool, std::error_code> vnamespace::unmount(std::filesystem::path old_path) noexcept {
  if (auto ec = normalize_path_in_place(old_path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto path_result = lookup_path(old_path);
  if (!path_result || !path_result.is_mount_point()) {
    return false;
  }
  if (path_result.is_mount_point_from_parent()) {
    // An inherited mount point can't be removed, only masked by a local mount point
    return std::unexpected(make_error_code(errc::unsupported));
  }

  // Remove entire mount point
  return mount_table_.remove(path_result.nid());
}

std::expected<void, std::error_code> vnamespace::unmount() noexcept {
  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  mount_table_.clear();
  return {};
}

// ------ file system apis

std::expected<node_status, std::error_code> vnamespace::status(std::filesystem::path path) const noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto result = lookup_path(path);
  switch (result.check_path()) {
  case lookup_path_result::path_check_result::dir:
  case lookup_path_result::path_check_result::union_dir:
    return node_status::directory;
  case lookup_path_result::path_check_result::file:
    return node_status::file;
  case lookup_path_result::path_check_result::not_found:
  case lookup_path_result::path_check_result::unsupported:
  default:
    return node_status::not_found;
  }
}

std::expected<bool, std::error_code> vnamespace::exists(std::filesystem::path path) const noexcept {
  auto result = status(std::move(path));
  if (!result) {
    return std::unexpected(result.error());
  }
  return result.value() != node_status::not_found;
}

std::expected<node_info, std::error_code> vnamespace::get_info(std::filesystem::path path) const noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto result = lookup_path(path);
  if (result.empty()) {
    node_info info{};
    info.status = node_status::not_found;
    return info;
  }

  if (result.is_mount_point()) {
    if (result.mount_point()->entry_size() == 0) {
      node_info info{};
      info.status = node_status::not_found;
      return info;
    }
    const auto& nref = result.mount_point()->entry_list()[0].nref();
    return nref.fs()->get_info(nref.path());
  }

  const auto& nref = result.nref();
  return nref.fs()->get_info(nref.path());
}

std::expected<std::vector<node_entry>, std::error_code> vnamespace::list(std::filesystem::path path) const noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto result = lookup_path(path);
  auto check = result.check_path();
  if (check != lookup_path_result::path_check_result::dir &&
      check != lookup_path_result::path_check_result::union_dir) {
    return std::unexpected(make_error_code(
        check == lookup_path_result::path_check_result::not_found ? errc::path_not_found : errc::not_a_dir));
  }

  std::vector<node_entry> out;
  detail::sv_set seen;
  auto append_from = [&](const detail::node_ref& nref) -> std::error_code {
    std::vector<node_entry> entries;
    if (auto list_result = nref.fs()->list(nref.path(), entries); !list_result) {
      return list_result.error();
    }
    for (auto& entry : entries) {
      if (seen.insert(entry.name).second) {
        out.push_back(std::move(entry));
      }
    }
    return {};
  };

  if (result.is_mount_point()) {
    // Union directory: merge the entries of all mount entries, higher priority wins.
    for (const auto& entry : result.mount_point()->entry_list()) {
      if (auto ec = append_from(entry.nref())) {
        return std::unexpected(std::move(ec));
      }
    }
  } else {
    detail::node_ref nref(result.nref());
    if (auto ec = append_from(nref)) {
      return std::unexpected(std::move(ec));
    }
  }
  return out;
}

std::expected<bool, std::error_code> vnamespace::exists(std::filesystem::path path, node_type nt) const noexcept {
  auto result = status(std::move(path));
  if (!result) {
    return std::unexpected(result.error());
  }
  switch (result.value()) {
  case node_status::directory:
    return nt == node_type::directory;
  case node_status::file:
    return nt == node_type::file;
  case node_status::not_found:
  default:
    return false;
  }
}

std::expected<void, std::error_code> vnamespace::create_directory(std::filesystem::path path, bool recursive) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto target = resolve_creation_target(path, recursive);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->first.fs()->create_directory(target->second, recursive);
}

std::expected<void, std::error_code> vnamespace::create_file(std::filesystem::path path,
                                                             placeholder_file_create_spec spec) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto target = resolve_creation_target(path, false);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->first.fs()->create_file(target->second, spec);
}

std::expected<void, std::error_code> vnamespace::create_file(std::filesystem::path path,
                                                             regular_file_create_spec spec) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto target = resolve_creation_target(path, false);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->first.fs()->create_file(target->second, spec);
}

std::expected<void, std::error_code> vnamespace::create_file(std::filesystem::path path,
                                                             scalar_file_create_spec spec) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto target = resolve_creation_target(path, false);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->first.fs()->create_file(target->second, spec);
}

std::expected<void, std::error_code> vnamespace::remove(std::filesystem::path path, bool recursive) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  unique_ns_lock lk(this);
  if (lk.error()) {
    return std::unexpected(lk.error());
  }

  auto result = lookup_path(path);
  auto target = result.resolve_node_ref(true);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->fs()->remove(target->path(), recursive);
}

std::expected<std::shared_ptr<stream_file>, std::error_code>
vnamespace::open_file(std::filesystem::path path, stream_file_open_options options) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto target = lookup_path(path).resolve_node_ref(options.mode != file_open_mode::read);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->fs()->open_file(target->path(), options);
}

std::expected<std::shared_ptr<seq_stream_file>, std::error_code>
vnamespace::open_file(std::filesystem::path path, seq_stream_file_open_options options) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto target = lookup_path(path).resolve_node_ref(options.mode != file_open_mode::read);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->fs()->open_file(target->path(), options);
}

std::expected<std::shared_ptr<mapped_file>, std::error_code>
vnamespace::open_file(std::filesystem::path path, mapped_file_open_options options) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto target = lookup_path(path).resolve_node_ref(options.mode != file_open_mode::read);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->fs()->open_file(target->path(), options);
}

std::expected<std::shared_ptr<scalar_file>, std::error_code>
vnamespace::open_file(std::filesystem::path path, scalar_file_open_options options) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto target = lookup_path(path).resolve_node_ref(options.mode != file_open_mode::read);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->fs()->open_file(target->path(), options);
}

std::expected<std::shared_ptr<control_file>, std::error_code>
vnamespace::open_file(std::filesystem::path path, control_file_open_options options) noexcept {
  if (auto ec = normalize_path_in_place(path)) {
    return std::unexpected(std::move(ec));
  }

  shared_ns_lock lk(this);
  auto target = lookup_path(path).resolve_node_ref(false);
  if (!target) {
    return std::unexpected(target.error());
  }
  return target->fs()->open_file(target->path(), options);
}

std::expected<size_t, std::error_code>
vnamespace::read_all(std::filesystem::path path,
                     mutable_buffer_sequence buffers,
                     seq_stream_file_open_options options) noexcept {
  options.mode = file_open_mode::read;
  auto file = open_file(std::move(path), options);
  if (!file) {
    return std::unexpected(file.error());
  }
  return file.value()->read(buffers);
}

std::expected<size_t, std::error_code>
vnamespace::write_all(std::filesystem::path path,
                      const_buffer_sequence buffers,
                      seq_stream_file_open_options options) noexcept {
  options.mode = file_open_mode::write;
  auto file = open_file(std::move(path), options);
  if (!file) {
    return std::unexpected(file.error());
  }
  return file.value()->write(buffers);
}

std::expected<std::any, std::error_code> vnamespace::read_scalar(std::filesystem::path path,
                                                                 scalar_file_open_options options) noexcept {
  options.mode = file_open_mode::read;
  auto file = open_file(std::move(path), options);
  if (!file) {
    return std::unexpected(file.error());
  }
  return file.value()->read();
}

std::expected<void, std::error_code> vnamespace::write_scalar(std::filesystem::path path,
                                                              std::any value,
                                                              scalar_file_open_options options) noexcept {
  options.mode = file_open_mode::write;
  auto file = open_file(std::move(path), options);
  if (!file) {
    return std::unexpected(file.error());
  }
  return file.value()->write(std::move(value));
}

std::expected<void, std::error_code> vnamespace::execute(std::filesystem::path path,
                                                         std::string_view command,
                                                         control_file_open_options options) noexcept {
  auto file = open_file(std::move(path), options);
  if (!file) {
    return std::unexpected(file.error());
  }
  return file.value()->execute(command);
}

} // namespace vfs9
