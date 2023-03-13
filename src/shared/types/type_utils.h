/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <arrow/type.h>

#include <memory>
#include <string>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace types {

inline std::string_view ToString(DataType type) { return magic_enum::enum_name(type); }
inline std::string_view ToString(types::SemanticType type) { return SemanticType_Name(type); }
inline std::string_view ToString(types::PatternType type) { return PatternType_Name(type); }

}  // namespace types
}  // namespace px

// Internal utility macro that creates a single case statement and calls the
// case macro for the type.
#define PX_SWITCH_FOREACH_DATATYPE_CASE(_dt_, _EXPR_MACRO_, _CASE_MACRO_) \
  case _EXPR_MACRO_(_dt_): {                                              \
    _CASE_MACRO_(_dt_);                                                   \
  } break

// Internal utility macro to generate the default case.
#define PX_SWITCH_FOREACH_DATATYPE_DEFAULT_CASE(_dt_) \
  default: {                                          \
    CHECK(0) << "Unknown Type: " << _dt_;             \
  }

#define PL_IDENT_EXPR(_dt_) _dt_

// Uses the Datatype as the constexpr to switch on.
#define PX_SWITCH_FOREACH_DATATYPE(_dt_, _CASE_MACRO_) \
  PX_SWITCH_FOREACH_DATATYPE_WITHEXPR(_dt_, PL_IDENT_EXPR, _CASE_MACRO_)

/**
 * PX_SWITCH_FOREACH_DATATYPE can be use to run a macro func over each data type we have. For
 * example:
 *
 * DataType dt = <...>;
 *
 * #define EXPR_CASE(_dt_) DataTypeTraits<_dt_>::arrow_type
 * #define TYPE_CASE(_dt_) ExtractFoo<_dt_>(...)
 * PX_SWITCH_FOREACH_DATATYPE_WITHEXPR(dt, EXPR_CASE, TYPE_CASE)
 * #undef EXPR_CASE
 * #undef TYPE_CASE
 *
 * Will run the function ExtractFoo with the correct args (at runtime).
 *
 * PX_CARNOT_UPDATE_FOR_NEW_TYPES.
 */
#define PX_SWITCH_FOREACH_DATATYPE_WITHEXPR(_dt_, _EXPR_MACRO_, _CASE_MACRO_)                      \
  do {                                                                                             \
    auto __dt_var__ = (_dt_);                                                                      \
    switch (__dt_var__) {                                                                          \
      PX_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::BOOLEAN, _EXPR_MACRO_, _CASE_MACRO_); \
      PX_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::INT64, _EXPR_MACRO_, _CASE_MACRO_);   \
      PX_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::UINT128, _EXPR_MACRO_, _CASE_MACRO_); \
      PX_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::TIME64NS, _EXPR_MACRO_,               \
                                      _CASE_MACRO_);                                               \
      PX_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::FLOAT64, _EXPR_MACRO_, _CASE_MACRO_); \
      PX_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::STRING, _EXPR_MACRO_, _CASE_MACRO_);  \
      PX_SWITCH_FOREACH_DATATYPE_DEFAULT_CASE(__dt_var__);                                         \
    }                                                                                              \
  } while (0)
