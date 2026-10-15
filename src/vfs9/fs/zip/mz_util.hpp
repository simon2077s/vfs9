/**
 * @file mz_util.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief minizip-ng helper utilities
 * @version 0.1
 * @date 2026-10-15
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <cstdint>
#include <system_error>

#include <mz.h>

#include "vfs9/error.hpp"

namespace vfs9 {
namespace fs {
namespace zip {

/**
 * @brief Convert a minizip-ng error code to a std::error_code
 *
 * @param error minizip-ng error code
 * @return std::error_code
 */
inline std::error_code mz_to_error(int32_t error) {
  switch (error) {
  case MZ_OK:
    return {};
  case MZ_END_OF_LIST:
  case MZ_END_OF_STREAM:
    return make_error_code(errc::path_not_found);
  case MZ_EXIST_ERROR:
    return make_error_code(std::errc::file_exists);
  case MZ_PARAM_ERROR:
    return make_error_code(std::errc::invalid_argument);
  case MZ_MEM_ERROR:
    return make_error_code(std::errc::not_enough_memory);
  default:
    return make_error_code(std::errc::io_error);
  }
}

} // namespace zip
} // namespace fs
} // namespace vfs9
