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
#include <arrow/builder.h>
#include <arrow/type.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <functional>

#include <absl/numeric/int128.h>
#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace types {

/**
 * The enum of different data types.
 * TODO(zasgar): move this data type to a higher lvl than plan.
 */
using DataType = types::DataType;

/**
 * This is the base value type that all value types inherit from.
 */
struct BaseValueType {};

/**
 * The value type for string values.
 */
struct StringValue : BaseValueType, public std::string {
  using std::string::string;
  // Allow implicit construction to make it easier/more natural to return values
  // from functions.
  // NOLINTNEXTLINE: implicit constructor.
  StringValue(std::string&& str) : std::string(std::move(str)) {}
  // NOLINTNEXTLINE: implicit constructor.
  StringValue(const std::string& str) : std::string(str) {}
  int64_t bytes() const { return sizeof(char) * this->length(); }

  const std::string& string() { return *this; }
};

/**
 * Defines the value type for all ValueTypes that have a fixed size in memory.
 * @tparam T The underlying data type.
 */
template <typename T>
struct FixedSizedValueType : BaseValueType {
  // We need this to make UBSAN happy since otherwise it's un-initialized.
  // TODO(zasgar): Understand if this impacts performance.
  T val = 0;
  constexpr FixedSizedValueType() : BaseValueType() {}

  // Allow implicit construction to make it easier/more natural to return values
  // from functions.
  // NOLINTNEXTLINE: implicit constructor.
  constexpr FixedSizedValueType(T new_val) : val(new_val) {}

  // GCC requires the copy constructor to be explicitly specified.
  // NOLINTNEXTLINE: implicit constructor.
  constexpr FixedSizedValueType(const FixedSizedValueType<T>& rhs) = default;

  template <class T2>
  // Overload the equality to make it easier to write code with value types.
  constexpr bool operator==(T2 rhs) const {
    return val == ExtractBaseValue(rhs);
  }

  template <class T2>
  // Overload the not equality to make it easier to write code with value types.
  constexpr bool operator!=(T2 rhs) const {
    return val != ExtractBaseValue(rhs);
  }

  // Overload > and < to make it easier to write code with value types.
  template <class T2>
  constexpr bool operator<(T2 rhs) const {
    return val < ExtractBaseValue(rhs);
  }

  template <class T2>
  constexpr bool operator<=(T2 rhs) const {
    return val <= ExtractBaseValue(rhs);
  }

  template <class T2>
  constexpr bool operator>(T2 rhs) const {
    return val > ExtractBaseValue(rhs);
  }

  template <class T2>
  constexpr bool operator>=(T2 rhs) const {
    return val >= ExtractBaseValue(rhs);
  }

  // Overload assignment to make it easier to write code with value types.
  FixedSizedValueType<T>& operator=(const FixedSizedValueType<T>& rhs) {
    val = rhs.val;
    return *this;
  }

  FixedSizedValueType<T>& operator=(const T& rhs) {
    val = rhs;
    return *this;
  }

  int64_t bytes() const { return sizeof(T); }

  StringValue Serialize() const {
    return StringValue(reinterpret_cast<const char*>(&val), sizeof(val));
  }

  Status Deserialize(const StringValue& in) {
    DCHECK(sizeof(val) == in.size());
    std::memcpy(reinterpret_cast<char*>(&val), in.data(), sizeof(val));
    return Status::OK();
  }

 private:
  /**
   * Extracts the value type based on if it's an integral type (int, float, etc.) or one of our
   * value types.
   */
  template <typename TExt>
  constexpr auto ExtractBaseValue(TExt v) const {
    if constexpr (std::is_arithmetic_v<TExt>) {
      return v;
    } else if constexpr (std::is_base_of_v<BaseValueType, TExt>) {
      return v.val;
    } else {
      static_assert(sizeof(TExt) == 0, "Unsupported extract type");
    }
  }
};

using BoolValue = FixedSizedValueType<bool>;
using Int64Value = FixedSizedValueType<int64_t>;
using Float64Value = FixedSizedValueType<double>;

struct UInt128Value : public FixedSizedValueType<absl::uint128> {
  using FixedSizedValueType::FixedSizedValueType;

  // NOLINTNEXTLINE: implicit constructor.
  UInt128Value(const UInt128& lhs) { val = absl::MakeUint128(lhs.high(), lhs.low()); }

  UInt128Value(uint64_t high, uint64_t low) { val = absl::MakeUint128(high, low); }

  uint64_t High64() const { return absl::Uint128High64(val); }

  uint64_t Low64() const { return absl::Uint128Low64(val); }

  bool operator==(const absl::uint128 other) const { return other == val; }

  bool operator==(const UInt128Value& other) const { return other.val == val; }

  bool operator!=(const absl::uint128 other) const { return other != val; }

  bool operator!=(const UInt128Value& other) const { return other.val != val; }
};

struct Time64NSValue : public Int64Value {
  using Int64Value::Int64Value;
  // Allow implicit construction to make it easier/more natural to return values
  // from functions and also in other code using int's for time.
  // NOLINTNEXTLINE: implicit constructor.
  Time64NSValue(int64_t lhs) : Int64Value(lhs) {}
};

union FixedSizeValueUnion {
  // Don't construct values up creation of this union, this default constructor
  // prevents construction of sub types.
  // NOLINTNEXTLINE: using = default does not work, we actually want an empty constructor.
  FixedSizeValueUnion(){};
  BoolValue bool_value;
  Int64Value int64_value;
  UInt128Value uint128_value;
  Float64Value float64_value;
  Time64NSValue time64ns_value;
};

const uint8_t kFixedSizeBytes = sizeof(FixedSizeValueUnion);

static_assert(kFixedSizeBytes == 16,
              "Please re-consider bloating fixed size values since it might have significant "
              "performance impact");

/**
 * Get the value out of the fixed sized union (by type).
 * @tparam T The type to pull out.
 * @param u The union.
 * @return const reference to the value.
 */
template <typename T>
inline const T& Get(const FixedSizeValueUnion& u) {
  PX_UNUSED(u);
  static_assert(sizeof(T) == 0, "Invalid type for get expression");
}

template <>
inline const BoolValue& Get<BoolValue>(const FixedSizeValueUnion& u) {
  return u.bool_value;
}

template <>
inline const Int64Value& Get<Int64Value>(const FixedSizeValueUnion& u) {
  return u.int64_value;
}

template <>
inline const UInt128Value& Get<UInt128Value>(const FixedSizeValueUnion& u) {
  return u.uint128_value;
}

template <>
inline const Float64Value& Get<Float64Value>(const FixedSizeValueUnion& u) {
  return u.float64_value;
}

template <>
inline const Time64NSValue& Get<Time64NSValue>(const FixedSizeValueUnion& u) {
  return u.time64ns_value;
}

template <typename T>
inline void SetValue(FixedSizeValueUnion* u, T val) {
  PX_UNUSED(u);
  PX_UNUSED(val);
  static_assert(sizeof(T) == 0, "Invalid type for set expression");
}

template <>
inline void SetValue<BoolValue>(FixedSizeValueUnion* u, BoolValue val) {
  DCHECK(u != nullptr);
  u->bool_value = val;
}

template <>
inline void SetValue<Int64Value>(FixedSizeValueUnion* u, Int64Value val) {
  DCHECK(u != nullptr);
  u->int64_value = val;
}

template <>
inline void SetValue<UInt128Value>(FixedSizeValueUnion* u, UInt128Value val) {
  DCHECK(u != nullptr);
  u->uint128_value = val;
}

template <>
inline void SetValue<Float64Value>(FixedSizeValueUnion* u, Float64Value val) {
  DCHECK(u != nullptr);
  u->float64_value = val;
}

template <>
inline void SetValue<Time64NSValue>(FixedSizeValueUnion* u, Time64NSValue val) {
  DCHECK(u != nullptr);
  u->time64ns_value = val;
}

/**
 * Checks to see if a valid ValueType is being used.
 * @tparam T The type to check.
 * PX_CARNOT_UPDATE_FOR_NEW_TYPES
 */
template <typename T>
struct IsValidValueType {
  static constexpr bool value =
      std::is_base_of_v<BaseValueType, T> &&
      (std::is_same_v<T, BoolValue> || std::is_same_v<T, Int64Value> ||
       std::is_same_v<T, Float64Value> || std::is_same_v<T, StringValue> ||
       std::is_same_v<T, Time64NSValue> || std::is_same_v<T, UInt128Value>);
};

/**
 * Get information about a particular ValueType. For example: mapping back to the
 * enum type.
 * @tparam T the ValueType.
 */
template <typename T>
struct ValueTypeTraits {
  static_assert(!IsValidValueType<T>::value, "Invalid ValueType.");
};

template <>
struct ValueTypeTraits<BoolValue> {
  static constexpr bool is_fixed_size = true;
  static constexpr DataType data_type = types::BOOLEAN;
  using arrow_type = arrow::BooleanType;
  using arrow_builder_type = arrow::BooleanBuilder;
  using arrow_array_type = arrow::BooleanArray;
  using native_type = bool;
};

template <>
struct ValueTypeTraits<Int64Value> {
  static constexpr bool is_fixed_size = true;
  static constexpr DataType data_type = types::INT64;
  using arrow_type = arrow::Int64Type;
  using arrow_builder_type = arrow::Int64Builder;
  using arrow_array_type = arrow::Int64Array;
  using native_type = int64_t;
};

template <>
struct ValueTypeTraits<UInt128Value> {
  static constexpr bool is_fixed_size = true;
  static constexpr DataType data_type = types::UINT128;
  using arrow_type = arrow::UInt128Type;
  using arrow_builder_type = arrow::UInt128Builder;
  using arrow_array_type = arrow::UInt128Type;
  using native_type = absl::uint128;
};

template <>
struct ValueTypeTraits<Float64Value> {
  static constexpr bool is_fixed_size = true;
  static constexpr DataType data_type = types::FLOAT64;
  using arrow_type = arrow::DoubleType;
  using arrow_builder_type = arrow::DoubleBuilder;
  using arrow_array_type = arrow::DoubleArray;
  using native_type = double;
};

template <>
struct ValueTypeTraits<Time64NSValue> {
  static constexpr bool is_fixed_size = true;
  static constexpr DataType data_type = types::TIME64NS;
  using arrow_type = arrow::Int64Type;
  using arrow_builder_type = arrow::Int64Builder;
  using arrow_array_type = arrow::Int64Array;
  using native_type = int64_t;
};

template <>
struct ValueTypeTraits<StringValue> {
  static constexpr bool is_fixed_size = false;
  static constexpr DataType data_type = types::STRING;
  using arrow_type = arrow::StringType;
  using arrow_builder_type = arrow::StringBuilder;
  using arrow_array_type = arrow::StringArray;
  using native_type = std::string;
};

/**
 * Store traits based on the native ValueType.
 * @tparam T The DataType.
 */
template <DataType T>
struct DataTypeTraits {};

template <>
struct DataTypeTraits<DataType::BOOLEAN> {
  using value_type = BoolValue;
  using arrow_type = arrow::BooleanType;
  using arrow_builder_type = arrow::BooleanBuilder;
  using arrow_array_type = arrow::BooleanArray;
  using native_type = bool;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::BOOL;
};

template <>
struct DataTypeTraits<DataType::INT64> {
  using value_type = Int64Value;
  using arrow_type = arrow::Int64Type;
  using arrow_builder_type = arrow::Int64Builder;
  using arrow_array_type = arrow::Int64Array;
  using native_type = int64_t;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::INT64;
};

template <>
struct DataTypeTraits<DataType::UINT128> {
  using value_type = UInt128Value;
  using arrow_type = arrow::UInt128Type;
  using arrow_builder_type = arrow::UInt128Builder;
  using arrow_array_type = arrow::UInt128Array;
  using native_type = absl::uint128;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::UINT128;
};

template <>
struct DataTypeTraits<DataType::FLOAT64> {
  using value_type = Float64Value;
  using arrow_type = arrow::DoubleType;
  using arrow_builder_type = arrow::DoubleBuilder;
  using arrow_array_type = arrow::DoubleArray;
  using native_type = double;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::DOUBLE;
};

template <>
struct DataTypeTraits<DataType::STRING> {
  using value_type = StringValue;
  using arrow_type = arrow::StringType;
  using arrow_builder_type = arrow::StringBuilder;
  using arrow_array_type = arrow::StringArray;
  using native_type = std::string;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::STRING;
};

template <>
struct DataTypeTraits<DataType::TIME64NS> {
  using value_type = Time64NSValue;
  using arrow_type = arrow::Time64Type;
  using arrow_builder_type = arrow::Time64Builder;
  using arrow_array_type = arrow::Time64Array;
  using native_type = int64_t;
  static inline std::shared_ptr<arrow::DataType> default_value() {
    return arrow::time64(arrow::TimeUnit::NANO);
  }
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::TIME64;
};

}  // namespace types
}  // namespace px
