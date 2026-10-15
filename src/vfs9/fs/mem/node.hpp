/**
 * @file node.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Memory file system node
 * @version 0.1
 * @date 2026-10-10
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <any>
#include <cstddef>
#include <optional>
#include <system_error>

#include <llfio/llfio.hpp>

#include "vfs9/fs.hpp"

namespace llfio = LLFIO_V2_NAMESPACE;

namespace vfs9 {
namespace fs {
namespace mem {

/**
 * @brief Memory file system node base
 *
 *  Stores the file specific payload of a node: directories leave every member at its default value.
 *
 */
class node_base {
public:
  node_base() = default;

  node_base(const node_base&) = delete;
  node_base(node_base&&) = default;

  ~node_base() = default;

  node_base& operator=(const node_base&) = delete;
  node_base& operator=(node_base&&) = default;

  /**
   * @brief Get file type
   *
   * @return file_type
   */
  inline file_type ft() const noexcept { return ft_; }

  /**
   * @brief Get logical file size
   *
   * @return size_t
   */
  inline size_t size() const noexcept { return size_; }

  /**
   * @brief Get the backing section of a regular file, may be nullptr
   *
   * @return llfio::section_handle*
   */
  inline llfio::section_handle* section() noexcept { return section_ ? &*section_ : nullptr; }

  /**
   * @brief Get the mapped view of a regular file, may be nullptr
   *
   * @return llfio::map_handle*
   */
  inline llfio::map_handle* map() noexcept { return map_ ? &*map_ : nullptr; }

  /**
   * @brief Get scalar value
   *
   * @return const std::any&
   */
  inline const std::any& scalar() const noexcept { return scalar_; }

  /**
   * @brief Get scalar value
   *
   * @return std::any&
   */
  inline std::any& scalar() noexcept { return scalar_; }

  /**
   * @brief Mark this node as a placeholder file
   *
   */
  inline void mark_placeholder() noexcept { ft_ = file_type::placeholder; }

  /**
   * @brief Mark this node as a scalar file
   *
   * @param value Initial value
   */
  inline void mark_scalar(std::any value) noexcept {
    ft_ = file_type::scalar;
    scalar_ = std::move(value);
  }

  /**
   * @brief Mark this node as a regular file
   *
   * @param size Initial logical size
   */
  inline void mark_regular(size_t size) noexcept {
    ft_ = file_type::regular;
    size_ = size;
  }

  /**
   * @brief Set logical file size
   *
   * @param size New size
   */
  inline void set_size(size_t size) noexcept { size_ = size; }

  /**
   * @brief Get the number of open file handles referencing this node
   *
   * @return size_t
   */
  inline size_t open_count() const noexcept { return open_count_; }

  /**
   * @brief Mark this node as referenced by one more open file handle
   *
   */
  inline void acquire() noexcept { ++open_count_; }

  /**
   * @brief Release one open file handle reference
   *
   */
  inline void release() noexcept {
    if (open_count_ > 0) {
      --open_count_;
    }
  }

  /**
   * @brief Ensure the backing section and its mapped view can hold at least `needed` bytes
   *
   *  Creates the anonymous section (and map) on first use, grows and remaps it when necessary.
   *
   * @param needed Required capacity in bytes
   * @return std::error_code
   */
  std::error_code ensure_capacity(size_t needed) noexcept;

private:
  file_type ft_ = file_type::placeholder;        // File type
  size_t size_ = 0;                              // Logical file size
  std::optional<llfio::section_handle> section_; // Regular file backing
  std::optional<llfio::map_handle> map_;         // Mapped view of section_
  std::any scalar_;                              // Scalar file value
  size_t open_count_ = 0;                        // Number of open file handles
};

} // namespace mem
} // namespace fs
} // namespace vfs9
