/**
 * @file error.cpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Error
 * @version 0.1
 * @date 2026-08-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "vfs9/error.hpp"

namespace vfs9 {

const error_category& error_category_instance() {
  static error_category instance;
  return instance;
}

} // namespace vfs9
