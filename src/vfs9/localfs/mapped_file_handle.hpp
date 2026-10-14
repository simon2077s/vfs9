/**
 * @file mapped_file_handle.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Mapped file handle
 * @version 0.1
 * @date 2026-10-14
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstddef>
#include <filesystem>
#include <span>

#include <llfio/llfio.hpp>

#include "vfs9/detail/type_traits.hpp"
#include "vfs9/localfs/base_file_handle.hpp"

namespace llfio = LLFIO_V2_NAMESPACE;

namespace vfs9 {
namespace localfs_impl {

/**
 * @brief Mapped file handle
 *
 */
class mapped_file_handle : public base_file_handle {
public:
  /**
   * @brief Construct a new mapped file handle object
   *
   * @param mapped Mapped file handle
   * @param local_path Local OS path
   * @param path Path relative to the file system root
   */
  mapped_file_handle(llfio::mapped_file_handle mapped, std::filesystem::path local_path, std::filesystem::path path);

  /**
   * @brief File max size
   *
   * @return size_t
   */
  size_t max_size() const noexcept;

  /**
   * @brief Get mapped data
   *
   * @return std::span<std::byte>
   */
  std::span<std::byte> data() noexcept;

  /**
   * @brief Get mapped data
   *
   * @return std::span<const std::byte>
   */
  std::span<const std::byte> data() const noexcept;

  /**
   * @brief Get address space reservation
   *
   * @return size_t
   */
  size_t reservation() const noexcept;

  /**
   * @brief Get offset into the backing file where the mapping begins
   *
   * @return size_t
   */
  size_t starting_offset() const noexcept;

private:
  llfio::mapped_file_handle mapped_; // Mapped file handle
};

static_assert(detail::is_fs_mapped_file_handle<mapped_file_handle>);

} // namespace localfs_impl
} // namespace vfs9
