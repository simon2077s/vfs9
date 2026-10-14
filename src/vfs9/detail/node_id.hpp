/**
 * @file node_id.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Node id
 * @version 0.1
 * @date 2026-10-01
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <boost/container_hash/hash.hpp>

namespace vfs9 {
namespace detail {

template <typename Path>
class basic_node_id {
  template <typename>
  friend class basic_node_id;

public:
  using path_type = Path;

  basic_node_id() = default;

  template <typename T>
  basic_node_id(uint32_t fs_id, T&& path) : fs_id_(fs_id), path_(std::forward<T>(path)) {}

  template <typename Path1>
  basic_node_id(const basic_node_id<Path1>& other) : fs_id_(other.fs_id_), path_(other.path_) {}

  basic_node_id(const basic_node_id&) = default;

  basic_node_id(basic_node_id&& other) : fs_id_(other.fs_id_), path_(std::move(other.path_)) { other.fs_id_ = 0; }

  ~basic_node_id() = default;

  basic_node_id& operator=(const basic_node_id&) = default;

  basic_node_id& operator=(basic_node_id&& other) {
    fs_id_ = other.fs_id_;
    path_ = std::move(other.path_);
    other.fs_id_ = 0;
    return *this;
  }

  template <typename U>
  bool operator==(const basic_node_id<U>& other) const noexcept {
    return fs_id_ == other.fs_id_ && path_ == other.path_;
  }

  template <typename U>
  bool operator!=(const basic_node_id<U>& other) const noexcept {
    return !(*this == other);
  }

  /**
   * @brief Whether this node id is empty
   *
   * @return true
   * @return false
   */
  bool empty() const noexcept { return fs_id_ == 0; }
  /**
   * @brief Get file system id
   *
   * @return uint32_t
   */
  uint32_t fs_id() const noexcept { return fs_id_; }
  /**
   * @brief Get path which's relative to file system root
   *
   */
  const path_type& path() const noexcept { return path_; }

private:
  uint32_t fs_id_;
  path_type path_;
};

/**
 * @brief Node id
 *
 */
using node_id = basic_node_id<std::string>;

/**
 * @brief Node id view
 *
 */
using node_id_view = basic_node_id<std::string_view>;

} // namespace detail
} // namespace vfs9

namespace std {

template <typename T>
struct hash<vfs9::detail::basic_node_id<T>> {
  size_t operator()(const vfs9::detail::basic_node_id<T>& v) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, v.fs_id());
    boost::hash_combine(seed, v.path());
    return seed;
  }
};

} // namespace std

namespace vfs9 {
namespace detail {

struct node_id_key_hash {
  using is_transparent = void;

  template <typename T>
  size_t operator()(const basic_node_id<T>& id) const noexcept {
    return std::hash<basic_node_id<T>>{}(id);
  }
};

struct node_id_key_eq {
  using is_transparent = void;

  template <typename T, typename U>
  bool operator()(const basic_node_id<T>& a, const basic_node_id<U>& b) const noexcept {
    return a == b;
  }
};

/**
 * @brief A special unordered_map that can be looked up by node_id_view
 *
 * @tparam T
 */
template <typename T>
using node_id_map = std::unordered_map<node_id, T, node_id_key_hash, node_id_key_eq>;

/**
 * @brief A special unordered_set that can be looked up by node_id_view
 *
 */
using node_id_set = std::unordered_set<node_id, node_id_key_hash, node_id_key_eq>;

} // namespace detail
} // namespace vfs9
