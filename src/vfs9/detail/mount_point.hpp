/**
 * @file mount_point.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mount point
 * @version 0.1
 * @date 2026-09-03
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <span>
#include <vector>

#include "vfs9/detail/mount_entry.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief Mount point
 *
 *  Union directory: A mount point is a union directory when it's containing more than one entry.
 *
 */
class mount_point {
public:
  /**
   * @brief Construct a new mount point object
   *
   */
  mount_point() = default;

  /**
   * @brief Construct a new mount point object
   *
   */
  explicit mount_point(std::initializer_list<mount_entry> init_entry_list) : entry_list_(init_entry_list) {}

  mount_point(const mount_point&) = default;
  mount_point(mount_point&&) = default;

  ~mount_point() = default;

  mount_point& operator=(const mount_point&) = default;
  mount_point& operator=(mount_point&&) = default;

  inline operator bool() const noexcept { return !empty(); }

  /**
   * @brief Is this empty
   *
   * @return bool
   */
  inline bool empty() const noexcept { return entry_list_.empty(); }

  /**
   * @brief Get mount entry list size
   *
   * @return size_t
   */
  inline size_t entry_size() const noexcept { return entry_list_.size(); }

  /**
   * @brief Is this a union directory
   *
   * @return bool
   */
  inline bool is_union_dir() const noexcept { return entry_list_.size() > 1; }

  /**
   * @brief Get mount entry at i
   *
   * @param i Index
   * @return const mount_entry&
   */
  inline const mount_entry& operator[](size_t i) const noexcept { return entry_list_[i]; }

  /**
   * @brief Get mount entry at i
   *
   * @param i Index
   * @return const mount_entry&
   */
  inline mount_entry& operator[](size_t i) noexcept { return entry_list_[i]; }

  /**
   * @brief Get mount entry list
   *
   * @return std::span<const mount_entry>
   */
  inline std::span<const mount_entry> entry_list() const noexcept { return entry_list_; }

  /**
   * @brief Get mount entry list
   *
   * @return std::span<const mount_entry>
   */
  inline std::span<mount_entry> entry_list() noexcept { return entry_list_; }

  /**
   * @brief Get first writable entry
   *
   * @return const mount_entry*
   */
  inline const mount_entry* first_writable_entry() const noexcept {
    for (const auto& entry : entry_list_) {
      if (entry.writable()) {
        return &entry;
      }
    }
    return nullptr;
  }

  /**
   * @brief Get first writable entry
   *
   * @return const mount_entry*
   */
  inline mount_entry* first_writable_entry() noexcept {
    for (auto& entry : entry_list_) {
      if (entry.writable()) {
        return &entry;
      }
    }
    return nullptr;
  }

  /**
   * @brief Add entry
   *
   * @param before Add entry before any other entries or not
   */
  template <typename T>
    requires(std::is_constructible_v<node_ref, T>)
  void add(bool before, T&& nref, bool writable, std::string_view name = {}) {
    if (before) {
      entry_list_.emplace(entry_list_.begin(), std::forward<T>(nref), writable, name);
    } else {
      entry_list_.emplace(entry_list_.end(), std::forward<T>(nref), writable, name);
    }
  }

  /**
   * @brief Add entries
   *
   * @param before Add entries before any other entries
   * @param begin Begin iterator
   * @param end End iterator
   */
  template <typename BeginIt, typename EndIt>
  void add(bool before, BeginIt&& begin, EndIt&& end) {
    auto size = std::distance(begin, end);
    if (size == 0) {
      return;
    }
    entry_list_.reserve(entry_list_.size() + size);
    if (before) {
      entry_list_.insert(entry_list_.begin(), begin, end);
    } else {
      entry_list_.insert(entry_list_.end(), begin, end);
    }
  }

  /**
   * @brief Add entries from another mount point by copying entries
   *
   * @param before Add entries before any other entries
   * @param other Other mount point
   */
  void add(bool before, const mount_point& other) {
    if (other.entry_size() == 0) {
      return;
    }
    entry_list_.reserve(entry_list_.size() + other.entry_size());
    if (before) {
      entry_list_.insert(entry_list_.begin(), other.entry_list_.begin(), other.entry_list_.end());
    } else {
      entry_list_.insert(entry_list_.end(), other.entry_list_.begin(), other.entry_list_.end());
    }
  }

  /**
   * @brief Add entries from another mount point by moving entries
   *
   * @param before Add entries before any other entries
   * @param other Other mount point
   */
  void add(bool before, mount_point&& other) {
    if (other.entry_size() == 0) {
      return;
    }
    entry_list_.reserve(entry_list_.size() + other.entry_size());
    if (before) {
      entry_list_.insert(entry_list_.begin(),
                         std::make_move_iterator(other.entry_list_.begin()),
                         std::make_move_iterator(other.entry_list_.end()));
    } else {
      entry_list_.insert(entry_list_.end(),
                         std::make_move_iterator(other.entry_list_.begin()),
                         std::make_move_iterator(other.entry_list_.end()));
    }
    other.entry_list_.clear();
    other.entry_list_.shrink_to_fit();
  }

  /**
   * @brief Remove entry by name
   *
   * @param name Name to remove, either mount name or bind source path
   * @return bool, removed or not
   */
  inline bool remove(std::string_view name) noexcept {
    if (name.empty() || entry_list_.size() == 0) {
      return false;
    }
    auto n = std::erase_if(entry_list_, [&](auto&& e) {
      return e.name() == name;
    });
    return n > 0;
  }

  /**
   * @brief Clear entry list
   *
   */
  inline void clear() noexcept { entry_list_.clear(); }

private:
  std::vector<mount_entry> entry_list_;
};

} // namespace detail
} // namespace vfs9
