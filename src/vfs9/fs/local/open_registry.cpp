/**
 * @file open_registry.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Open registry
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/fs/local/open_registry.hpp"

namespace vfs9 {
namespace fs {
namespace local {

void open_registry::acquire(const std::filesystem::path& path) {
  std::lock_guard lock(mutex_);
  ++counts_[path];
}

void open_registry::release(const std::filesystem::path& path) {
  std::lock_guard lock(mutex_);
  auto it = counts_.find(path);
  if (it != counts_.end() && --it->second == 0) {
    counts_.erase(it);
  }
}

bool open_registry::has_open_at_or_below(const std::filesystem::path& path) const {
  std::lock_guard lock(mutex_);
  if (counts_.empty()) {
    return false;
  }
  const std::string base = path.generic_string();
  for (const auto& [key, count] : counts_) {
    if (count == 0) {
      continue;
    }
    const std::string candidate = key.generic_string();
    if (candidate == base) {
      return true;
    }
    if (base == "/" || (candidate.size() > base.size() && candidate.compare(0, base.size(), base) == 0 &&
                        candidate[base.size()] == '/')) {
      return true;
    }
  }
  return false;
}

} // namespace local
} // namespace fs
} // namespace vfs9
