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

#include <arrow/array.h>
#include <arrow/builder.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include <absl/numeric/int128.h>
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace types {

using arrow::Type;
DataType ArrowToDataType(const arrow::Type::type& arrow_type) {
#define EXPR_CASE(_dt_) DataTypeTraits<_dt_>::arrow_type_id
#define TYPE_CASE(_dt_) return _dt_;
  PX_SWITCH_FOREACH_DATATYPE_WITHEXPR(arrow_type, EXPR_CASE, TYPE_CASE);
#undef EXPR_CASE
#undef TYPE_CASE
}

arrow::Type::type ToArrowType(const DataType& udf_type) {
#define TYPE_CASE(_dt_) return DataTypeTraits<_dt_>::arrow_type_id;
  PX_SWITCH_FOREACH_DATATYPE(udf_type, TYPE_CASE);
#undef TYPE_CASE
}

int64_t ArrowTypeToBytes(const arrow::Type::type& arrow_type) {
#define EXPR_CASE(_dt_) DataTypeTraits<_dt_>::arrow_type_id
#define TYPE_CASE(_dt_) return sizeof(DataTypeTraits<_dt_>::native_type);
  PX_SWITCH_FOREACH_DATATYPE_WITHEXPR(arrow_type, EXPR_CASE, TYPE_CASE);
#undef EXPR_CASE
#undef TYPE_CASE
}

#define BUILDER_CASE(__data_type__, __pool__) \
  case __data_type__:                         \
    return std::make_unique<DataTypeTraits<__data_type__>::arrow_builder_type>(__pool__)

std::unique_ptr<arrow::ArrayBuilder> MakeArrowBuilder(const DataType& data_type,
                                                      arrow::MemoryPool* mem_pool) {
#define TYPE_CASE(_dt_) return GetArrowBuilder<_dt_>(mem_pool);
  PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE
}

#undef BUILDER_CASE

std::unique_ptr<TypeErasedArrowBuilder> MakeTypeErasedArrowBuilder(const DataType& data_type,
                                                                   arrow::MemoryPool* mem_pool) {
#define TYPE_CASE(_dt_)                                 \
  auto arrow_builder = GetArrowBuilder<_dt_>(mem_pool); \
  return std::unique_ptr<TypeErasedArrowBuilder>(       \
      new TypeErasedArrowBuilderImpl<_dt_>(std::move(arrow_builder)));
  PX_SWITCH_FOREACH_DATATYPE(data_type, TYPE_CASE);
#undef TYPE_CASE
}

}  // namespace types
}  // namespace px
