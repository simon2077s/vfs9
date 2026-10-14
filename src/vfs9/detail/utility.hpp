/**
 * @file utility.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Utility
 * @version 0.1
 * @date 2026-08-31
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace vfs9 {
namespace detail {

/**
 * @brief String view hash
 *
 */
struct sv_hash {
  using is_transparent = void;
  size_t operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
  size_t operator()(const std::string& s) const noexcept { return std::hash<std::string_view>{}(s); }
};

/**
 * @brief String view equality
 *
 */
struct sv_eq {
  using is_transparent = void;
  bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
  bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
  bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
  bool operator()(const std::string& a, const std::string& b) const noexcept { return a == b; }
};

/**
 * @brief A special unordered_map that can be looked up by string_view
 *
 * @tparam T
 */
template <typename T>
using sv_map = std::unordered_map<std::string, T, sv_hash, sv_eq>;

/**
 * @brief A special unordered_set that can be looked up by string_view
 *
 */
using sv_set = std::unordered_set<std::string, sv_hash, sv_eq>;

/**
 * @brief Make path object by range
 *
 * @param begin
 * @param end
 * @return std::filesystem::path
 */
inline std::filesystem::path make_path_from_range(const std::filesystem::path::const_iterator& begin,
                                                  const std::filesystem::path::const_iterator& end) {
  std::filesystem::path result;
  for (auto it = begin; it != end; ++it) {
    result /= *it;
  }
  return result;
}

/**
 * @brief Make path object by range
 *
 * @param begin
 * @param end
 * @return std::filesystem::path
 */
inline std::filesystem::path make_path_from_range(const std::filesystem::path& prefix,
                                                  const std::filesystem::path::const_iterator& begin,
                                                  const std::filesystem::path::const_iterator& end) {
  std::filesystem::path result = prefix;
  for (auto it = begin; it != end; ++it) {
    result /= *it;
  }
  return result;
}

} // namespace detail
} // namespace vfs9
