/**
 * @file base_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Base file handle
 * @version 0.1
 * @date 2026-10-13
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <filesystem>
#include <memory>

#include "vfs9/detail/node_tree.hpp"
#include "vfs9/fs.hpp"
#include "vfs9/memfs/node.hpp"

namespace vfs9 {
namespace memfs_impl {

/**
 * @brief Base file handle
 *
 */
class base_file_handle {
public:
  using node_pointer = std::shared_ptr<detail::node_tree_node<node_base>>;

  /**
   * @brief Construct a new base file handle object
   *
   * @param node Node
   * @param path Path relative to the file system root
   */
  base_file_handle(node_pointer node, std::filesystem::path path);

  base_file_handle(const base_file_handle&) = delete;
  base_file_handle(base_file_handle&&) = default;

  virtual ~base_file_handle() = default;

  base_file_handle& operator=(const base_file_handle&) = delete;
  base_file_handle& operator=(base_file_handle&&) = default;

  /**
   * @brief Get the info
   *
   * @return node_info
   */
  node_info get_info() const noexcept;

protected:
  node_pointer node_;          // Node
  std::filesystem::path path_; // Path relative to the file system root
};

} // namespace memfs_impl
} // namespace vfs9
