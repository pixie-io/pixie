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

#include "third_party/arrow/cpp/src/arrow/type.h"

#include <memory>
#include <string>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace types {

inline std::string_view ToString(DataType type) { return magic_enum::enum_name(type); }
inline std::string_view ToString(types::SemanticType type) { return SemanticType_Name(type); }

inline std::shared_ptr<arrow::DataType> DataTypeToArrowType(DataType type) {
  switch (type) {
    case DataType::INT64:
      return arrow::int64();
    case DataType::UINT128:
      return arrow::uint128();
    case DataType::FLOAT64:
      return arrow::float64();
    case DataType::TIME64NS:
      return arrow::time64(arrow::TimeUnit::NANO);
    case DataType::STRING:
      return arrow::utf8();
    case DataType::BOOLEAN:
      return arrow::boolean();
    default:
      DCHECK(false) << absl::StrFormat("Unknown data type %s", ToString(type));
      return nullptr;
  }
}

}  // namespace types
}  // namespace px

// Internal utility macro that creates a single case statement and calls the
// case macro for the type.
#define PL_SWITCH_FOREACH_DATATYPE_CASE(_dt_, _CASE_MACRO_) \
  case _dt_: {                                              \
    _CASE_MACRO_(_dt_);                                     \
  } break

// Internal utility macro to generate the default case.
#define PL_SWITCH_FOREACH_DATATYPE_DEFAULT_CASE(_dt_)            \
  default: {                                                     \
    CHECK(0) << "Unknown Type: " << ::px::types::ToString(_dt_); \
  }

/**
 * PL_SWITCH_FOREACH_DATATYPE can be use to run a macro func over each data type we have. For
 * example:
 *
 * DataType dt = <...>;
 *
 * #define TYPE_CASE(_dt_) ExtractFoo<_dt_>(...)
 * PL_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE)
 * #undef TYPE_CASE
 *
 * Will run the function ExtractFoo with the correct args (at runtime).
 *
 * PL_CARNOT_UPDATE_FOR_NEW_TYPES.
 */
#define PL_SWITCH_FOREACH_DATATYPE(_dt_, _CASE_MACRO_)                                \
  do {                                                                                \
    auto __dt_var__ = (_dt_);                                                         \
    switch (__dt_var__) {                                                             \
      PL_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::BOOLEAN, _CASE_MACRO_);  \
      PL_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::INT64, _CASE_MACRO_);    \
      PL_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::UINT128, _CASE_MACRO_);  \
      PL_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::TIME64NS, _CASE_MACRO_); \
      PL_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::FLOAT64, _CASE_MACRO_);  \
      PL_SWITCH_FOREACH_DATATYPE_CASE(::px::types::DataType::STRING, _CASE_MACRO_);   \
      PL_SWITCH_FOREACH_DATATYPE_DEFAULT_CASE(__dt_var__);                            \
    }                                                                                 \
  } while (0)
