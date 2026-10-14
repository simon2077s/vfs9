/**
 * @file lock.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief
 * @version 0.1
 * @date 2026-10-08
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <atomic>
#include <optional>
#include <shared_mutex>
#include <system_error>
#include <utility>

#include "vfs9/error.hpp"

namespace vfs9 {
namespace detail {

/**
 * @brief Freezable mutex
 *
 */
class freezable_mutex {
public:
  /**
   * @brief Construct a new freezable mutex object
   *
   */
  freezable_mutex() noexcept : frozen_(false) {}

  /**
   * @brief Freeze the mutex. After calling this method, try to unique lock this mutex will cause an error.
   *
   */
  inline void freeze() noexcept {
    std::unique_lock lk(m_); // Ensure that no other unique lock is still alive
    {
      frozen_ = true;
    }
  }
  /**
   * @brief Is mutex frozen
   *
   * @return true
   * @return false
   */
  inline bool is_frozen() const noexcept { return frozen_.load(std::memory_order_acquire); }

private:
  friend class shared_freezable_lock;
  friend class unique_freezable_lock;

  std::atomic<bool> frozen_;
  mutable std::shared_mutex m_;
};

/**
 * @brief Shared freezable lock
 *
 */
class shared_freezable_lock {
public:
  explicit shared_freezable_lock(const freezable_mutex& m) : lk_() {
    if (m.is_frozen()) {
      return;
    }
    lk_.emplace(m.m_);
    // Re-check while holding the lock: freeze() may have completed between the
    // check above and the acquisition, in which case the lock must be dropped.
    if (m.is_frozen()) {
      lk_.reset();
    }
  }

  ~shared_freezable_lock() = default;

private:
  std::optional<std::shared_lock<std::shared_mutex>> lk_;
};

/**
 * @brief Unique freezable lock
 *
 */
class unique_freezable_lock {
public:
  explicit unique_freezable_lock(const freezable_mutex& m) : ec_(), lk_() {
    if (m.is_frozen()) {
      ec_ = make_error_code(errc::frozen);
      return;
    }
    lk_.emplace(m.m_);
    // Re-check while holding the lock: freeze() may have completed between the
    // check above and the acquisition, in which case the write lock must not be
    // granted after the mutex has been frozen.
    if (m.is_frozen()) {
      lk_.reset();
      ec_ = make_error_code(errc::frozen);
    }
  }

  ~unique_freezable_lock() = default;

  /**
   * @brief Get error
   *
   * @return const std::error_code&
   */
  inline const std::error_code& error() const noexcept { return ec_; }

  /**
   * @brief Get movable error
   *
   * @return std::error_code&&
   */
  inline std::error_code&& error() noexcept { return std::move(ec_); }

private:
  std::error_code ec_;
  std::optional<std::unique_lock<std::shared_mutex>> lk_;
};

} // namespace detail
} // namespace vfs9
