/**
 * @file node_tree.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Node tree
 * @version 0.1
 * @date 2026-10-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "vfs9/detail/utility.hpp"
#include "vfs9/fs.hpp"

namespace vfs9 {
namespace detail {

template <typename Base>
class node_tree_node;

/**
 * @brief Node tree name -> node map. This class implements soo algorithm.
 *
 *  NOTE: This class is not thread-safe
 *
 */
template <typename NodeBase>
class node_tree_node_map {
public:
  using node_type = std::shared_ptr<node_tree_node<NodeBase>>;

  node_tree_node_map() = default;

  node_tree_node_map(const node_tree_node_map&) = delete;
  node_tree_node_map(node_tree_node_map&& other) : container_(std::move(other.container_)) {
    other.container_ = std::monostate{};
  }

  ~node_tree_node_map() = default;

  node_tree_node_map& operator=(const node_tree_node_map&) = delete;

  node_tree_node_map& operator=(node_tree_node_map&& other) {
    container_ = std::move(other.container_);
    other.container_ = std::monostate{};
    return *this;
  }

  /**
   * @brief Whether this map is empty or not
   *
   * @return bool
   */
  bool empty() const noexcept {
    if (std::holds_alternative<std::monostate>(container_)) {
      return true;
    }
    if (const auto* p_array = std::get_if<array_type>(&container_)) {
      for (const auto& slot : *p_array) {
        if (slot) {
          return false;
        }
      }
      return true;
    }
    return std::get<map_type>(container_).empty();
  }

  /**
   * @brief Get node size
   *
   * @return size_t
   */
  size_t size() const noexcept {
    if (std::holds_alternative<std::monostate>(container_)) {
      return 0;
    }
    if (const auto* p_array = std::get_if<array_type>(&container_)) {
      size_t count = 0;
      for (const auto& slot : *p_array) {
        if (slot) {
          ++count;
        }
      }
      return count;
    }
    return std::get<map_type>(container_).size();
  }

  /**
   * @brief Check whether this map contains a name
   *
   * @param name Name to check
   * @return bool, contains or not
   */
  bool contains(std::string_view name) const noexcept {
    if (std::holds_alternative<std::monostate>(container_)) {
      return false;
    }
    if (const auto* p_array = std::get_if<array_type>(&container_)) {
      for (const auto& slot : *p_array) {
        if (slot && std::get<0>(*slot) == name) {
          return true;
        }
      }
      return false;
    }
    const auto& map = std::get<map_type>(container_);
    return map.find(name) != map.end();
  }

  /**
   * @brief Find node with name
   *
   * @param name Name
   * @return node_type
   */
  node_type find(std::string_view name) const noexcept {
    if (std::holds_alternative<std::monostate>(container_)) {
      return nullptr;
    }
    if (const auto* p_array = std::get_if<array_type>(&container_)) {
      for (const auto& slot : *p_array) {
        if (slot && std::get<0>(*slot) == name) {
          return std::get<1>(*slot);
        }
      }
      return nullptr;
    }
    const auto& map = std::get<map_type>(container_);
    auto it = map.find(name);
    return it == map.end() ? nullptr : it->second;
  }

  /**
   * @brief Visit all nodes
   *
   * @tparam F Visit function type, signature is `void(std::string_view name, const node_type& p_node)`
   * @param f Visit function
   */
  template <typename F>
    requires(std::is_same_v<std::invoke_result_t<std::remove_cvref_t<F>, std::string_view, const node_type&>, void>)
  void for_each(F&& f) const noexcept {
    if (std::holds_alternative<std::monostate>(container_)) {
      return;
    }
    if (const auto* p_array = std::get_if<array_type>(&container_)) {
      for (const auto& slot : *p_array) {
        if (slot) {
          f(std::string_view(std::get<0>(*slot)), std::get<1>(*slot));
        }
      }
      return;
    }
    for (const auto& [name, p_node] : std::get<map_type>(container_)) {
      f(std::string_view(name), p_node);
    }
  }

  /**
   * @brief Add or update a new node
   *
   * @param name Node name
   * @param p_node New node
   * @param overwrite Overwrite existing node if exists, false by default (which will return false with nothing
   * changed)
   * @return bool, set or not
   */
  bool set(std::string_view name, node_type p_node, bool overwrite = false) {
    if (std::holds_alternative<std::monostate>(container_)) {
      array_type array{};
      array[0].emplace(std::string(name), std::move(p_node));
      container_ = std::move(array);
      return true;
    }
    if (auto* p_array = std::get_if<array_type>(&container_)) {
      for (auto& slot : *p_array) {
        if (slot && std::get<0>(*slot) == name) {
          if (!overwrite) {
            return false;
          }
          std::get<1>(*slot) = std::move(p_node);
          return true;
        }
      }
      for (auto& slot : *p_array) {
        if (!slot) {
          slot.emplace(std::string(name), std::move(p_node));
          return true;
        }
      }
      // The array is full, promote to a hash map.
      map_type map;
      map.reserve(array_size + 1);
      for (auto& slot : *p_array) {
        if (slot) {
          map.emplace(std::move(std::get<0>(*slot)), std::move(std::get<1>(*slot)));
        }
      }
      map.emplace(std::string(name), std::move(p_node));
      container_ = std::move(map);
      return true;
    }
    auto& map = std::get<map_type>(container_);
    auto it = map.find(name);
    if (it != map.end()) {
      if (!overwrite) {
        return false;
      }
      it->second = std::move(p_node);
      return true;
    }
    map.emplace(std::string(name), std::move(p_node));
    return true;
  }

  /**
   * @brief Erase a node
   *
   * @param name The name of node to erase
   * @return bool, erased or not
   */
  bool erase(std::string_view name) {
    if (std::holds_alternative<std::monostate>(container_)) {
      return false;
    }
    if (auto* p_array = std::get_if<array_type>(&container_)) {
      for (auto& slot : *p_array) {
        if (slot && std::get<0>(*slot) == name) {
          slot.reset();
          if (empty()) {
            container_ = std::monostate{};
          }
          return true;
        }
      }
      return false;
    }
    auto& map = std::get<map_type>(container_);
    auto it = map.find(name);
    if (it == map.end()) {
      return false;
    }
    map.erase(it);
    if (map.empty()) {
      container_ = std::monostate{};
    } else if (map.size() <= array_size) {
      // Demote back to the small array representation.
      array_type array{};
      size_t i = 0;
      for (auto& [key, p_node] : map) {
        array[i++].emplace(std::move(key), std::move(p_node));
      }
      container_ = std::move(array);
    }
    return true;
  }

  /**
   * @brief Erase all nodes
   *
   */
  void clear() { container_ = std::monostate{}; }

private:
  static constexpr size_t array_size = 32;

  /**
   * @brief When the number of children is <= 32, then std::array will be used, otherwise map will be used (and
   * std::monostate for empty children).
   *
   */
  using array_type = std::array<std::optional<std::tuple<std::string, node_type>>, array_size>;
  using map_type = detail::sv_map<node_type>;
  using container_type = std::variant<std::monostate, array_type, map_type>;

  container_type container_;
};

/**
 * @brief Node of node tree, represents directory and file
 *
 *  NOTE: This class is not thread-safe
 *
 */
template <typename Base>
class node_tree_node : public Base {
public:
  /**
   * @brief Construct a new node tree node object
   *
   * @tparam Ts Base constructor argument types
   * @param type Node type
   * @param name Node name
   * @param args Base constructor arguments
   */
  template <typename... Ts>
  explicit node_tree_node(vfs9::node_type type, std::string name, Ts&&... args)
      : Base(std::forward<Ts>(args)...), type_(type), name_(std::move(name)) {}

  /**
   * @brief Get node type
   *
   * @return vfs9::node_type
   */
  vfs9::node_type type() const noexcept { return type_; }

  /**
   * @brief Get node name
   *
   * @return const std::string&
   */
  const std::string& name() const noexcept { return name_; }

  /**
   * @brief Get children map, while file node shouldn't have any children usually
   *
   * @return const node_tree_node_map<Base>&
   */
  const node_tree_node_map<Base>& children() const noexcept { return children_; }

  /**
   * @brief Get children map, while file node shouldn't have any children usually
   *
   * @return node_tree_node_map<Base>&
   */
  node_tree_node_map<Base>& children() noexcept { return children_; }

private:
  vfs9::node_type type_;
  std::string name_;
  node_tree_node_map<Base> children_;
};

/**
 * @brief Node tree
 *
 *  NOTE: This class is not thread-safe
 *
 * @tparam NodeBase
 */
template <typename NodeBase>
class node_tree {
public:
  using node_type = std::shared_ptr<node_tree_node<NodeBase>>;

  node_tree() = default;

  /**
   * @brief Construct a new node tree object
   *
   * @param p_root Root node
   */
  explicit node_tree(node_type p_root) : p_root_(std::move(p_root)) {}

  /**
   * @brief Get root node
   *
   * @return node_type
   */
  node_type root() const noexcept { return p_root_; }

  /**
   * @brief Whether this tree contains a path from begin to end, will return end if this tree contains the whole path
   *
   * @param begin Begin iterator
   * @param end End iterator
   * @return Iterator, the iterator after the last found node
   */
  template <typename Iterator>
  Iterator contains(const Iterator& begin, Iterator& end) const noexcept {
    auto p_node = p_root_;
    for (auto it = begin; it != end; ++it) {
      if (!p_node) {
        return it;
      }
      std::string_view name = node_name(*it);
      if (name == "/") {
        continue;
      }
      p_node = p_node->children().find(name);
      if (!p_node) {
        return it;
      }
    }
    return end;
  }

  /**
   * @brief Find the node with a path
   *
   * @param path Path
   * @return node_type
   */
  node_type find(const std::filesystem::path& path) const noexcept {
    auto begin = path.begin();
    auto end = path.end();
    return find(begin, end);
  }

  /**
   * @brief Find the descendant node of `from` by walking path components from begin to end
   *
   * @param from Starting node
   * @param begin Begin iterator
   * @param end End iterator
   * @return node_type, `from` itself when the range is empty
   */
  template <typename Iterator>
  static node_type find(node_type from, const Iterator& begin, const Iterator& end) noexcept {
    auto p_node = std::move(from);
    for (auto it = begin; it != end && p_node; ++it) {
      std::string_view name = node_name(*it);
      if (name == "/") {
        continue;
      }
      p_node = p_node->children().find(name);
    }
    return p_node;
  }

  /**
   * @brief Find the node with path from begin to end
   *
   * @param begin Begin iterator
   * @param end End iterator
   * @return node_type
   */
  template <typename Iterator>
  node_type find(const Iterator& begin, Iterator& end) const noexcept {
    auto p_node = p_root_;
    for (auto it = begin; it != end; ++it) {
      if (!p_node) {
        return nullptr;
      }
      std::string_view name = node_name(*it);
      if (name == "/") {
        continue;
      }
      p_node = p_node->children().find(name);
    }
    return p_node;
  }

  /**
   * @brief Set the node to path, will return false if path not found
   *
   * @param begin Begin iterator
   * @param end End iterator
   * @param node Node to set
   * @param overwrite Overwite or not
   * @return bool, updated or not
   */
  template <typename Iterator>
  bool set(const Iterator& begin, Iterator& end, node_type node, bool overwrite = false) {
    if (begin == end) {
      return false;
    }
    auto parent_end = end;
    --parent_end;
    auto p_parent = find(begin, parent_end);
    if (!p_parent) {
      return false;
    }
    std::string_view name = node_name(*parent_end);
    if (name == "/") {
      return false;
    }
    return p_parent->children().set(name, std::move(node), overwrite);
  }

  /**
   * @brief Set the node to path recursively, will call new_dir_func to create missing dir nodes
   *
   * @param begin Begin iterator
   * @param end End iterator
   * @param new_dir_func Function to create new dir node
   * @param node Node to set
   * @param overwrite Overwite or not
   * @return bool, updated or not
   */
  template <typename Iterator, typename NewMiddleNodeFunc>
    requires(std::is_same_v<std::invoke_result_t<std::remove_cvref_t<NewMiddleNodeFunc>, std::string_view>, node_type>)
  bool recursive_set(
      const Iterator& begin, Iterator& end, NewMiddleNodeFunc&& new_dir_func, node_type node, bool overwrite = false) {
    if (begin == end) {
      return false;
    }
    auto p_node = p_root_;
    auto last = end;
    --last;
    for (auto it = begin; it != last; ++it) {
      if (!p_node) {
        return false;
      }
      std::string_view name = node_name(*it);
      if (name == "/") {
        continue;
      }
      auto child = p_node->children().find(name);
      if (!child) {
        child = new_dir_func(name);
        if (!child) {
          return false;
        }
        p_node->children().set(name, child, false);
      }
      p_node = child;
    }
    if (!p_node) {
      return false;
    }
    std::string_view name = node_name(*last);
    if (name == "/") {
      return false;
    }
    return p_node->children().set(name, std::move(node), overwrite);
  }

  /**
   * @brief Erase node of path, if the node is a dir than recursive must be true
   *
   * @param begin Begin iterator
   * @param end End iterator
   * @param recursive Recursively earse the node
   * @return bool, erased or not
   */
  template <typename Iterator>
  bool erase(const Iterator& begin, Iterator& end, bool recursive = false) {
    if (begin == end) {
      return false;
    }
    auto parent_end = end;
    --parent_end;
    auto p_parent = find(begin, parent_end);
    if (!p_parent) {
      return false;
    }
    std::string_view name = node_name(*parent_end);
    if (name == "/") {
      return false;
    }
    auto child = p_parent->children().find(name);
    if (!child) {
      return false;
    }
    if (!recursive && !child->children().empty()) {
      return false;
    }
    return p_parent->children().erase(name);
  }

  /**
   * @brief Erase all nodes
   *
   */
  void clear() {
    if (p_root_) {
      p_root_->children().clear();
    }
  }

private:
  /**
   * @brief Get the string view of a path component
   *
   * @tparam T Path component type
   * @param value Path component
   * @return std::string_view
   */
  template <typename T>
  static std::string_view node_name(const T& value) noexcept {
    if constexpr (std::is_convertible_v<const T&, std::string_view>) {
      return std::string_view(value);
    } else {
      return std::string_view(value.c_str());
    }
  }

  node_type p_root_; // Root node
};

} // namespace detail
} // namespace vfs9
