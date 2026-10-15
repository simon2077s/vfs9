/**
 * @file error.hpp
 * @author simon2077s (simon2077s@gmail.com)
 * @brief Error
 * @version 0.1
 * @date 2026-08-24
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

#include <string>
#include <system_error>

#include <boost/describe.hpp>

namespace vfs9 {

/**
 * @brief Error code
 *
 */
BOOST_DEFINE_ENUM_CLASS(errc,
                        ok,
                        invalid_name,
                        frozen,
                        inited,
                        not_inited,
                        unsupported,
                        path_not_found,
                        empty_path,
                        invalid_path,
                        too_complex_path,
                        not_an_abs_path,
                        not_a_rel_path,
                        not_a_file,
                        not_a_dir,
                        not_an_union_dir,
                        not_a_dir_or_union_dir,
                        different_node_type,
                        must_be_standalone_ns,
                        scalar_value_type_mismatch,
                        file_in_use);

/**
 * @brief Error category
 *
 */
class error_category : public std::error_category {
public:
  const char* name() const noexcept override { return "vfs9 error"; }

  std::string message(int ev) const override {
    return boost::describe::enum_to_string(static_cast<errc>(ev), "unknown");
  }
};

/**
 * @brief Get error category instance
 *
 * @return const error_category&
 */
const error_category& error_category_instance();

/**
 * @brief Make error code
 *
 * @param e Error code
 * @return std::error_code
 */
inline std::error_code make_error_code(errc e) { return { static_cast<int>(e), error_category_instance() }; }

/**
 * @brief Make error condition
 *
 * @param e Error code
 * @return std::error_condition
 */
inline std::error_condition make_error_condition(errc e) { return { static_cast<int>(e), error_category_instance() }; }

} // namespace vfs9

namespace std {

template <>
struct is_error_code_enum<vfs9::errc> : true_type {};

} // namespace std
