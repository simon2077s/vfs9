/**
 * @file mount_table.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mount table. Current implementation is as simple as a hash table.
 * @version 0.1
 * @date 2026-08-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <sstream>
#include <vector>

#include "vfs9/detail/mount_point.hpp"
#include "vfs9/detail/node_id.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief The result of lookup mount point
 *
 */
class lookup_mount_point_result {
public:
  /**
   * @brief Construct a new lookup mount point result object
   *
   */
  lookup_mount_point_result() = default;

  /**
   * @brief Construct a new lookup mount point result object
   *
   * @param nid Node id
   * @param p_mp Mount point pointer
   * @param from_parent Is mount point from parent
   */
  lookup_mount_point_result(node_id_view nid, const mount_point* p_mp, bool from_parent)
      : nid_(nid), p_mp_(p_mp), from_parent_(from_parent) {}

  lookup_mount_point_result(const lookup_mount_point_result&) = default;

  lookup_mount_point_result(lookup_mount_point_result&& other)
      : nid_(other.nid_), p_mp_(other.p_mp_), from_parent_(other.from_parent_) {
    other.p_mp_ = nullptr;
    other.from_parent_ = false;
  }

  lookup_mount_point_result& operator=(const lookup_mount_point_result&) = default;

  inline lookup_mount_point_result& operator=(lookup_mount_point_result&& other) {
    nid_ = std::move(other.nid_);
    p_mp_ = other.p_mp_;
    from_parent_ = other.from_parent_;
    other.p_mp_ = nullptr;
    other.from_parent_ = false;
    return *this;
  }

  inline operator bool() const noexcept { return !empty(); }

  inline bool empty() const noexcept { return p_mp_ == nullptr || p_mp_->entry_size() == 0; }

  /**
   * @brief Get node id
   *
   * @return node_id_view
   */
  inline node_id_view nid() const noexcept { return nid_; }

  /**
   * @brief Get mount point pointer
   *
   * @return const mount_point*
   */
  inline const mount_point* mp() const noexcept { return p_mp_; }

  /**
   * @brief Is mount point from parent
   *
   * @return bool
   */
  inline bool from_parent() const noexcept { return from_parent_; }

private:
  node_id_view nid_;
  const mount_point* p_mp_;
  bool from_parent_;
};

/**
 * @brief Mount table
 *
 */
class mount_table {
public:
  /**
   * @brief Construct a new mount table object
   *
   */
  mount_table() : p_parent_(nullptr) {}

  /**
   * @brief Construct a new mount table object
   *
   * @param p_parent Parent mount table
   */
  mount_table(const mount_table* p_parent) : p_parent_(p_parent) {}

  mount_table(const mount_table&) = default;

  mount_table(mount_table&& other) : container_(std::move(other.container_)), p_parent_(other.p_parent_) {
    other.p_parent_ = nullptr;
  }

  ~mount_table() = default;

  mount_table& operator=(const mount_table&) = default;

  inline mount_table& operator=(mount_table&& other) {
    container_ = std::move(other.container_);
    p_parent_ = other.p_parent_;
    other.p_parent_ = nullptr;
    return *this;
  }

  /**
   * @brief lookup mount point
   *
   * @param nid Node id view
   * @return lookup_mount_point_result
   */
  inline lookup_mount_point_result lookup(node_id_view nid) const noexcept {
    auto result = local_lookup(nid, false);
    if (!result && p_parent_) {
      return p_parent_->local_lookup(nid, true);
    }
    return result;
  }

  /**
   * @brief Lookup nearest mount point
   *
   * @param fs_id File system id
   * @param begin Begin iterator of the searching path component
   * @param end End iterator of the searching path component
   * @return std::tuple<lookup_mount_point_result, std::filesystem::path::const_iterator>
   */
  inline std::tuple<lookup_mount_point_result, std::filesystem::path::const_iterator>
  lookup_nearest(uint32_t fs_id,
                 const std::filesystem::path::const_iterator& begin,
                 const std::filesystem::path::const_iterator& end) const noexcept {
    if (begin == end || *begin != "/") {
      return std::make_tuple(lookup_mount_point_result(), begin);
    }

    std::ostringstream oss;
    for (auto it = begin; it != end; ++it) {
      oss << (*it).string();
      auto lookup_result = lookup(node_id_view(fs_id, oss.view()));
      if (lookup_result) {
        // Found a mount point
        return std::make_tuple(std::move(lookup_result), ++it);
      }
      // Not found, append a back slash and try next element
      if (*it != "/") {
        oss << '/';
      }
    }

    return std::make_tuple(lookup_mount_point_result(), begin);
  }

  /**
   * @brief Lookup nearest mount point
   *
   * @param fs_id File system id
   * @param prefix Path prefix which must be included in the searching path
   * @param begin Begin iterator of the searching path component
   * @param end End iterator of the searching path component
   * @return std::tuple<lookup_mount_point_result, std::filesystem::path::const_iterator>
   */
  inline std::tuple<lookup_mount_point_result, std::filesystem::path::const_iterator>
  lookup_nearest(uint32_t fs_id,
                 const std::filesystem::path& prefix,
                 const std::filesystem::path::const_iterator& begin,
                 const std::filesystem::path::const_iterator& end) const noexcept {
    // begin -> end can start with / only if prefix is /
    if (begin == end || (prefix != "/" && *begin == "/")) {
      return std::make_tuple(lookup_mount_point_result(), begin);
    }

    std::ostringstream oss;
    if (prefix != "/") {
      oss << prefix.string() << '/';
    } else if (*begin != "/") {
      oss << '/';
    }
    // NOTE: If prefix is / and begin -> end is /aa/bb/..., the prefix is ignored in this case.
    for (auto it = begin; it != end; ++it) {
      oss << (*it).string();
      auto lookup_result = lookup(node_id_view(fs_id, oss.view()));
      if (lookup_result) {
        // Found a mount point
        return std::make_tuple(std::move(lookup_result), ++it);
      }
      // Not found, append a back slash and try next element
      if (*it != "/") {
        oss << '/';
      }
    }

    return std::make_tuple(lookup_mount_point_result(), end);
  }

  /**
   * @brief Assign a node id with mount point
   *
   * @param nid Node id
   * @param point Mount point
   * @return mount_point&, the inserted or updated mount entry object
   */
  template <typename T, typename U>
    requires(std::is_same_v<std::remove_cvref_t<U>, detail::mount_point>)
  mount_point& assign(T&& nid, U&& mp) {
    return std::get<0>(container_.insert_or_assign(std::forward<T>(nid), std::forward<U>(mp)))->second;
  }

  /**
   * @brief Assign a node id with a one node mount point
   *
   * @param nid Node id
   * @param nref Node reference
   * @param writable Writable or not
   * @param name Optionl name
   * @return mount_point&, the inserted or updated mount entry object
   */
  template <typename T, typename U>
  mount_point& assign(T&& nid, U&& nref, bool writable, std::string_view name = {}) {
    return std::get<0>(container_.insert_or_assign(
                           std::forward<T>(nid),
                           detail::mount_point({ detail::mount_entry(std::forward<U>(nref), writable, name) })))
        ->second;
  }

  /**
   * @brief Assign a node id with mount entry iterator
   *
   * @param nid Node id
   * @param begin Mount entry begin iterator
   * @param end Mount entry end iterator
   * @return mount_point&
   */
  template <typename T, typename BeginIt, typename EndIt>
  mount_point& assign(T&& nid, BeginIt&& begin, EndIt&& end) {
    mount_point mp;
    mp.add(false, begin, end);
    return std::get<0>(container_.insert_or_assign(std::forward<T>(nid), std::move(mp)))->second;
  }

  /**
   * @brief Remove mount entry
   *
   * @param nid Node id
   * @return bool, removed or not
   */
  inline bool remove(node_id_view nid) noexcept {
    auto it = container_.find(nid);
    if (it != container_.end()) {
      container_.erase(it);
      return true;
    }
    return false;
  }

  /**
   * @brief Clear the mount table
   *
   */
  inline void clear() { container_.clear(); }

  /**
   * @brief Flatten this mount table with its parent chain into a standalone mount table
   *
   *  The parent entries are merged from the root of the parent chain to this table, so the entries
   *  of this table always take precedence over the inherited ones. The returned mount table has no
   *  parent, which makes it suitable to build a standalone (cloned) namespace.
   *
   * @return mount_table
   */
  mount_table flatten() const {
    std::vector<const mount_table*> chain;
    for (const mount_table* p = this; p != nullptr; p = p->p_parent_) {
      chain.push_back(p);
    }
    mount_table result(nullptr);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
      for (const auto& [nid, mp] : (*it)->container_) {
        result.container_.insert_or_assign(nid, mp);
      }
    }
    return result;
  }

private:
  inline lookup_mount_point_result local_lookup(node_id_view nid, bool set_from_parent) const noexcept {
    auto it = container_.find(nid);
    if (it != container_.end()) {
      return lookup_mount_point_result(it->first, &it->second, set_from_parent);
    }
    return {};
  }

  node_id_map<mount_point> container_; // Node id to mount point
  const mount_table* p_parent_;
};

} // namespace detail
} // namespace vfs9
