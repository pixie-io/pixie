#pragma once
#include <arrow/builder.h>
#include <arrow/type.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <functional>

#include "src/common/base/base.h"
#include "src/shared/types/proto/types.pb.h"

namespace pl {
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
 * Defines the value type for all ValueTypes that have a fixed size in memory.
 * @tparam T The underlying data type.
 */
template <typename T>
struct FixedSizedValueType : BaseValueType {
  // We need this to make UBSAN happy since otherwise it's un-initialized.
  // TODO(zasgar): Understand if this impacts performance.
  T val = 0;
  FixedSizedValueType() : BaseValueType() {}

  // Allow implicit construction to make it easier/more natural to return values
  // from functions.
  // NOLINTNEXTLINE: implicit constructor.
  FixedSizedValueType(T new_val) : val(new_val) {}

  template <class T2>
  // Overload the equality to make it easier to write code with value types.
  bool operator==(const FixedSizedValueType<T2>& lhs) const {
    return val == lhs.val;
  }
  template <class T2>
  bool operator==(const T2& lhs) const {
    return val == lhs;
  }

  // Overload > and < to make it easier to write code with value types.
  template <class T2>
  bool operator<(const FixedSizedValueType<T2>& lhs) const {
    return val < lhs.val;
  }
  template <class T2>
  bool operator<(const T2& lhs) const {
    return val < lhs;
  }
  template <class T2>
  bool operator>(const FixedSizedValueType<T2>& lhs) const {
    return val > lhs.val;
  }
  template <class T2>
  bool operator>(const T2& lhs) const {
    return val > lhs;
  }

  // Overload assignment to make it easier to write code with value types.
  FixedSizedValueType<T>& operator=(FixedSizedValueType<T> lhs) {
    val = lhs.val;
    return *this;
  }
  FixedSizedValueType<T>& operator=(T lhs) {
    val = lhs;
    return *this;
  }

  int64_t bytes() const { return sizeof(T); }
};

using BoolValue = FixedSizedValueType<bool>;
using Int64Value = FixedSizedValueType<int64_t>;
using Float64Value = FixedSizedValueType<double>;

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
  Float64Value float64_value;
  Time64NSValue time64ns_value;
};

const uint8_t kFixedSizeBytes = sizeof(FixedSizeValueUnion);

static_assert(kFixedSizeBytes == 8,
              "Please re-consider bloating fixed size values since it might have significant "
              "performance impact");
/**
 * The value type for string values.
 */
struct StringValue : BaseValueType, public std::string {
  using std::string::string;
  // Allow implicit construction to make it easier/more natural to return values
  // from functions.
  // NOLINTNEXTLINE: implicit constructor.
  StringValue(std::string&& str) : std::string(std::move(str)) {}
  int64_t bytes() const { return sizeof(char) * this->length(); }
};

/**
 * Get the value out of the fixed sized union (by type).
 * @tparam T The type to pull out.
 * @param u The union.
 * @return const reference to the value.
 */
template <typename T>
inline const T& Get(const FixedSizeValueUnion& u) {
  PL_UNUSED(u);
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
inline const Float64Value& Get<Float64Value>(const FixedSizeValueUnion& u) {
  return u.float64_value;
}

template <>
inline const Time64NSValue& Get<Time64NSValue>(const FixedSizeValueUnion& u) {
  return u.time64ns_value;
}

template <typename T>
inline void SetValue(FixedSizeValueUnion* u, T val) {
  PL_UNUSED(u);
  PL_UNUSED(val);
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
 * PL_CARNOT_UPDATE_FOR_NEW_TYPES
 */
template <typename T>
struct IsValidValueType {
  static constexpr bool value =
      std::is_base_of_v<BaseValueType, T> &&
      (std::is_same_v<T, BoolValue> || std::is_same_v<T, Int64Value> ||
       std::is_same_v<T, Float64Value> || std::is_same_v<T, StringValue> ||
       std::is_same_v<T, Time64NSValue>);
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
  using arrow_type = arrow::Int64Type;
  using arrow_builder_type = arrow::Int64Builder;
  using arrow_array_type = arrow::Int64Array;
  using native_type = int64_t;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::INT64;
};

}  // namespace types
}  // namespace pl
