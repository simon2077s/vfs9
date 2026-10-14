/**
 * @file node_ref.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Node reference and node reference view
 * @version 0.1
 * @date 2026-08-31
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <filesystem>

#include "vfs9/detail/fs.hpp"
#include "vfs9/detail/node_id.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief Node ref view
 *
 */
class node_ref_view;

/**
 * @brief Node ref
 *
 *  Node ref stores a file system reference, so a node_ref object can reference to a node permanently
 *
 */
class node_ref {
public:
  /**
   * @brief Construct a new basic node ref object
   *
   */
  node_ref() = default;
  /**
   * @brief Construct a new basic node ref object
   *
   * @param p_fs File system shared pointer
   * @param path Path in file system
   */
  template <typename T, typename U>
    requires(std::is_constructible_v<std::shared_ptr<detail::file_system>, T> &&
             std::is_constructible_v<std::filesystem::path, U>)
  node_ref(T&& p_fs, U&& path) : p_fs_(std::forward<T>(p_fs)), path_(std::forward<U>(path)) {}

  node_ref(const node_ref_view& ref);

  node_ref(const node_ref&) = default;
  node_ref(node_ref&& other) = default;

  ~node_ref() = default;

  node_ref& operator=(const node_ref&) = default;
  node_ref& operator=(node_ref&& other) = default;

  inline node_ref& operator=(const node_ref_view& ref);

  friend inline bool operator==(const node_ref& a, const node_ref& b) noexcept {
    return a.p_fs_ == b.p_fs_ && a.path_ == b.path_;
  }

  /**
   * @brief Is empty
   *
   * @return bool
   */
  inline bool empty() const noexcept { return p_fs_ == nullptr; }

  /**
   * @brief Get file system
   *
   * @return file_system&
   */
  inline const std::shared_ptr<file_system>& fs() const noexcept { return p_fs_; }

  /**
   * @brief Get path in file system
   *
   * @return const std::filesystem::path&
   */
  inline const std::filesystem::path& path() const noexcept { return path_; }

  /**
   * @brief Create a new node id view object
   *
   * @return node_id_view
   */
  inline node_id_view nid() const noexcept { return node_id_view(p_fs_->id(), path_.c_str()); }

private:
  friend node_ref_view;

  std::shared_ptr<file_system> p_fs_;
  std::filesystem::path path_;
};

/**
 * @brief Node ref view
 *
 *  Node ref view doesn't own the reference to fs nor path, so you can't use this object to store the reference
 * permanently.
 *
 */
class node_ref_view {
public:
  /**
   * @brief Construct a new basic node ref object
   *
   */
  node_ref_view() = default;

  /**
   * @brief Construct a new node ref view object
   *
   * @param p_fs File system shared pointer
   * @param p_path Pointer of path in file system
   */
  template <typename T>
    requires(std::is_constructible_v<std::shared_ptr<detail::file_system>, T>)
  node_ref_view(T&& p_fs, const std::filesystem::path* p_path) : p_fs_(p_fs), p_path_(p_path) {}

  /**
   * @brief Construct a new node ref view object
   *
   * @param p_fs File system shared pointer
   * @param path Path in file system
   */
  template <typename T>
    requires(std::is_constructible_v<std::shared_ptr<detail::file_system>, T>)
  node_ref_view(T&& p_fs, const std::filesystem::path& path) : p_fs_(p_fs), p_path_(&path) {}

  /**
   * @brief Construct a new node ref view object
   *
   * @param ref Node ref object
   */
  node_ref_view(const node_ref& ref) : p_fs_(ref.p_fs_), p_path_(&ref.path_) {}

  node_ref_view(const node_ref_view&) = default;

  node_ref_view(node_ref_view&& other) : p_fs_(other.p_fs_), p_path_(other.p_path_) { other.p_path_ = nullptr; }

  ~node_ref_view() = default;

  node_ref_view& operator=(const node_ref_view&) = default;

  inline node_ref_view& operator=(node_ref_view&& other) {
    p_fs_ = other.p_fs_;
    p_path_ = other.p_path_;
    other.p_path_ = nullptr;
    return *this;
  }

  inline node_ref_view& operator=(const node_ref& other) {
    p_fs_ = other.p_fs_;
    p_path_ = &other.path_;
    return *this;
  }

  friend inline bool operator==(const node_ref_view& a, const node_ref_view& b) noexcept {
    return a.p_fs_ == b.p_fs_ && a.p_path_ == b.p_path_;
  }

  /**
   * @brief Is empty
   *
   * @return bool
   */
  inline bool empty() const noexcept { return p_fs_ == nullptr; }

  /**
   * @brief Get file system
   *
   * @return file_system&
   */
  inline const std::shared_ptr<file_system>& fs() const noexcept { return p_fs_; }

  /**
   * @brief Get path in file system
   *
   * @return const std::filesystem::path&
   */
  inline const std::filesystem::path& path() const noexcept { return *p_path_; }

  /**
   * @brief Create a new node id view object
   *
   * @return node_id_view
   */
  inline node_id_view nid() const noexcept { return node_id_view(p_fs_->id(), p_path_->c_str()); }

private:
  friend node_ref;

  std::shared_ptr<file_system> p_fs_;
  const std::filesystem::path* p_path_;
};

/**
 * @brief Construct a new node ref::node ref object
 *
 * @param ref Node ref view
 */
inline node_ref::node_ref(const node_ref_view& ref)
    : p_fs_(ref.empty() ? nullptr : ref.fs())
    , path_((ref.empty() || ref.p_path_ == nullptr) ? std::filesystem::path() : *ref.p_path_) {}

inline node_ref& node_ref::operator=(const node_ref_view& ref) {
  p_fs_ = ref.empty() ? nullptr : ref.fs();
  path_ = (ref.empty() || ref.p_path_ == nullptr) ? std::filesystem::path() : *ref.p_path_;
  return *this;
}

} // namespace detail
} // namespace vfs9
