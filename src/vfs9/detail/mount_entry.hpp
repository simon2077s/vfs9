/**
 * @file mount_entry.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mount entry
 * @version 0.1
 * @date 2026-10-01
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <string>

#include "vfs9/detail/node_ref.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief Mount entry
 *
 */
class mount_entry {
public:
  /**
   * @brief Construct a new (empty) mount entry object
   *
   */
  mount_entry() = default;

  /**
   * @brief Construct a new mount entry object
   *
   * @param nref Node reference
   * @param writable Writable or not
   * @param name Name of this entry, can be either mount name or bind source path
   */
  template <typename T>
    requires(std::is_constructible_v<node_ref, T>)
  mount_entry(T&& nref, bool writable, std::string_view name = {})
      : nref_(std::forward<T>(nref)), writable_(writable), name_(name) {}

  mount_entry(const mount_entry&) = default;
  mount_entry(mount_entry&& other) = default;

  ~mount_entry() = default;

  mount_entry& operator=(const mount_entry&) = default;
  mount_entry& operator=(mount_entry&&) = default;

  /**
   * @brief Is empty
   *
   * @return bool
   */
  inline bool empty() const noexcept { return nref_.empty(); }

  /**
   * @brief Create a new node ref
   *
   * @return node_ref
   */
  inline const node_ref& nref() const noexcept { return nref_; }

  /**
   * @brief Get writable or not
   *
   * @return bool
   */
  inline bool writable() const noexcept { return writable_; }

  /**
   * @brief Get (optional) name
   *
   * @return const std::string&
   */
  inline const std::string& name() const noexcept { return name_; }

private:
  node_ref nref_;
  bool writable_;
  std::string name_;
};

} // namespace detail
} // namespace vfs9
