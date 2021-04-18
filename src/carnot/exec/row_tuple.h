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

#include <arrow/array.h>
#include <farmhash.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/substitute.h>

#include "src/common/base/base.h"
#include "src/common/base/hash_utils.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/hash_utils.h"
#include "src/shared/types/type_utils.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace exec {

using VariableSizeValueTypeVariant = std::variant<types::StringValue>;

struct RowTuple;

namespace internal {
template <typename T>
inline void SetValueHelper(RowTuple* rt, size_t idx, const T& val);
template <>
inline void SetValueHelper<types::StringValue>(RowTuple* rt, size_t idx,
                                               const types::StringValue& val);

template <typename T>
inline const T& GetValueHelper(const RowTuple& rt, size_t idx);
template <>
inline const types::StringValue& GetValueHelper<types::StringValue>(const RowTuple& rt, size_t idx);

}  // namespace internal

/**
 * RowTuple stores a tuple of values corresponding to ValueTypes.
 */
struct RowTuple : public NotCopyable {
  explicit RowTuple(const std::vector<types::DataType>* types) : types(types) {
    Reset();
    // We set the values to zero since not all fixed size values are the same size.
    // Without this when we write values we might leave gaps that introduce mismatches during
    // comparisons, etc.
    memset(reinterpret_cast<uint8_t*>(fixed_values.data()), 0,
           sizeof(types::FixedSizeValueUnion) * fixed_values.size());
  }

  void Reset() {
    fixed_values.resize(types->size());
    variable_values.clear();
  }

  /**
   * Sets the value at the given index.
   * @tparam T The type value.
   * @param idx The index in the tuple.
   * @param val The value.
   */
  template <typename T>
  void SetValue(size_t idx, const T& val) {
    internal::SetValueHelper<T>(this, idx, val);
  }

  /**
   * Gets the value at the given index with type specified.
   * Will die in debug mode if wrong type is specified.
   * @tparam T The type to read as.
   * @param idx The index to read.
   * @return The return value (as reference).
   */
  template <typename T>
  const T& GetValue(size_t idx) const {
    return internal::GetValueHelper<T>(*this, idx);
  }

  bool operator==(const RowTuple& other) const {
    DCHECK(types != nullptr);
    DCHECK(other.types != nullptr);
    // This should actually be part of the check, but we assume that row-tuples
    // are consistently using the same types when they are being compared.
    DCHECK(*types == *(other.types));
    DCHECK(fixed_values.size() == other.fixed_values.size());
    DCHECK(types->size() == fixed_values.size());
    DCHECK(CheckSequentialWriteOrder()) << "Variable sized write ordering mismatch";
    DCHECK(other.CheckSequentialWriteOrder()) << "Variable sized write ordering mismatch";

    if (memcmp(fixed_values.data(), other.fixed_values.data(),
               sizeof(types::FixedSizeValueUnion) * fixed_values.size()) != 0) {
      // Early exit for failed comparisons.
      return false;
    }
    // Do  deep compare of the variable sized values;
    for (size_t idx = 0; idx < variable_values.size(); ++idx) {
      // This will invoke the correct operator on the variant.
      if (variable_values[idx] != other.variable_values[idx]) {
        return false;
      }
    }
    return true;
  }

  /**
   * Compute the hash of this RowTuple.
   *
   * @return the hash results.
   */
  size_t Hash() const {
    DCHECK(CheckSequentialWriteOrder()) << "Variable sized write ordering mismatch";
    // For fixed sized data we just hash the stored array.
    size_t hash = ::util::Hash64(reinterpret_cast<const char*>(fixed_values.data()),
                                 sizeof(types::FixedSizeValueUnion) * fixed_values.size());
    // For variable sized data we run the appropriate hash function.
    for (const auto& val : variable_values) {
      // This should be edited when we add support for new variable sized types.
      DCHECK(std::holds_alternative<types::StringValue>(val));
      hash = ::px::HashCombine(
          hash, types::utils::hash<types::StringValue>()(std::get<types::StringValue>(val)));
    }
    return hash;
  }

  /**
   * Checks to make sure the write order of variable sized data is sequential, implying that
   * the variable_sized data is in the correct order.
   * @return false if the write ordering is bad.
   */
  bool CheckSequentialWriteOrder() const {
    DCHECK(types != nullptr);
    DCHECK(types->size() >= fixed_values.size());
    int64_t expected_seq_id = 0;
    for (size_t idx = 0; idx < fixed_values.size(); ++idx) {
      // TODO(zasgar): Replace with IsVariableSizedType().
      if (types->at(idx) == types::STRING) {
        auto actual_seq_id = types::Get<types::Int64Value>(fixed_values[idx]).val;
        if (actual_seq_id != expected_seq_id) {
          LOG(ERROR) << absl::Substitute("Expected seq_id: $0, got $1", expected_seq_id,
                                         actual_seq_id);
          return false;
        }
        ++expected_seq_id;
      }
    }
    return true;
  }

  // We store a pointer to the types, since they are likely to be shared across different RowTuples.
  // This pointer needs to be valid for the lifetime of this object.
  const std::vector<types::DataType>* types;

  // We store data in two chunks. The first being fixed size values which store values of fixed size
  // values in line, and for variable size data stores the index into the variables_values array.
  // This index is stored as a Int64Value.
  std::vector<types::FixedSizeValueUnion> fixed_values;
  std::vector<VariableSizeValueTypeVariant> variable_values;
};

namespace internal {
template <typename T>
inline void SetValueHelper(RowTuple* rt, size_t idx, const T& val) {
  static_assert(types::ValueTypeTraits<T>::is_fixed_size, "Only fixed size values allowed");
  types::SetValue<T>(&rt->fixed_values[idx], val);
}

template <>
inline void SetValueHelper<types::StringValue>(RowTuple* rt, size_t idx,
                                               const types::StringValue& val) {
  DCHECK_LT(idx, rt->fixed_values.size());
  rt->SetValue(idx, types::Int64Value(rt->variable_values.size()));
  rt->variable_values.emplace_back(val);
}

template <typename T>
inline const T& GetValueHelper(const RowTuple& rt, size_t idx) {
  static_assert(types::ValueTypeTraits<T>::is_fixed_size, "Only fixed size values allowed");
  DCHECK_LT(idx, rt.types->size());
  DCHECK_EQ(types::ValueTypeTraits<T>::data_type, rt.types->at(idx));
  return types::Get<T>(rt.fixed_values[idx]);
}

template <>
inline const types::StringValue& GetValueHelper<types::StringValue>(const RowTuple& rt,
                                                                    size_t idx) {
  DCHECK_LT(idx, rt.types->size());
  DCHECK_EQ(types::ValueTypeTraits<types::StringValue>::data_type, rt.types->at(idx));
  size_t v_offset = types::Get<types::Int64Value>(rt.fixed_values[idx]).val;
  DCHECK_LT(v_offset, rt.variable_values.size());
  return std::get<types::StringValue>(rt.variable_values[v_offset]);
}
}  // namespace internal

/**
 * Hash operator for RowTuple pointers.
 */
struct RowTuplePtrHasher {
  size_t operator()(const RowTuple* k) const {
    DCHECK(k != nullptr);
    return k->Hash();
  }
};

/**
 * Equality operator for RowTuple pointers.
 */
struct RowTuplePtrEq {
  bool operator()(const RowTuple* k1, const RowTuple* k2) const { return *k1 == *k2; }
};

template <class T>
using AbslRowTupleHashMap = absl::flat_hash_map<RowTuple*, T, RowTuplePtrHasher, RowTuplePtrEq>;

using AbslRowTupleHashSet = absl::flat_hash_set<RowTuple*, RowTuplePtrHasher, RowTuplePtrEq>;

template <types::DataType DT>
void ExtractIntoRowTuple(RowTuple* rt, arrow::Array* col, int rt_col_idx, int rt_row_idx) {
  using UDFValueType = typename types::DataTypeTraits<DT>::value_type;
  using ArrowArrayType = typename types::DataTypeTraits<DT>::arrow_array_type;
  rt->SetValue<UDFValueType>(rt_col_idx,
                             types::GetValue(static_cast<ArrowArrayType*>(col), rt_row_idx));
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
