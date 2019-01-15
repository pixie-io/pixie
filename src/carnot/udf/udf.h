#pragma once
#include <glog/logging.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <functional>

#include "src/carnot/plan/proto/plan.pb.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace udf {

/**
 * The enum of different UDF data types.
 * TODO(zasgar): move this data type to a higher lvl than plan.
 */
using UDFDataType = planpb::DataType;

/**
 * This is the base value type that all UDFs inherit from.
 */
struct UDFBaseValue {};

/**
 * Defines the value tyoe for all UDF Values that have a fixed size in memory.
 * @tparam T The underlying data type.
 */
template <typename T>
struct FixedSizedUDFValue : UDFBaseValue {
  T val;
  FixedSizedUDFValue() : UDFBaseValue() {}

  // Allow implicit construction to make it easier/more natural to return values
  // from functions.
  // NOLINTNEXTLINE(runtime/explicit)
  FixedSizedUDFValue(T new_val) : val(new_val) {}
  // Overload the equality to make it easier to write code with value types.
  bool operator==(const FixedSizedUDFValue<T>& lhs) { return val == lhs.val; }
  bool operator==(const T& lhs) { return val == lhs; }
  // Overload assignment to make it easier to write code with value types.
  FixedSizedUDFValue<T>& operator=(FixedSizedUDFValue<T> lhs) {
    val = lhs.val;
    return *this;
  }
  FixedSizedUDFValue<T>& operator=(T lhs) {
    val = lhs;
    return *this;
  }
};

using BoolValue = FixedSizedUDFValue<bool>;
using Int64Value = FixedSizedUDFValue<int64_t>;
using Float64Value = FixedSizedUDFValue<double>;

/**
 * The value type for string values.
 */
struct StringValue : UDFBaseValue, std::string {
  using std::string::string;
};

/**
 * Checks to see if a valid UDF data type is being used.
 * @tparam T The type to check.
 */
template <typename T>
struct IsValidUDFDataType {
  static constexpr bool value = std::is_base_of_v<UDFBaseValue, T> &&
                                (std::is_same_v<T, BoolValue> || std::is_same_v<T, Int64Value> ||
                                 std::is_same_v<T, Float64Value> || std::is_same_v<T, StringValue>);
};

/**
 * Get information about a particular UDF value. For example: mapping back to the
 * enum type.
 * @tparam T the UDF value.
 */
template <typename T>
struct UDFValueTraits {
  static_assert(!IsValidUDFDataType<T>::value, "Invalid UDF data type.");
};

template <>
struct UDFValueTraits<BoolValue> {
  static constexpr UDFDataType data_type = planpb::BOOLEAN;
};

template <>
struct UDFValueTraits<Int64Value> {
  static constexpr UDFDataType data_type = planpb::INT64;
};

template <>
struct UDFValueTraits<Float64Value> {
  static constexpr UDFDataType data_type = planpb::FLOAT64;
};

template <>
struct UDFValueTraits<StringValue> {
  static constexpr UDFDataType data_type = planpb::STRING;
};

/**
 * Function context contains contextual resources such as mempools that functions
 * can use while executing.
 */
class FunctionContext {};

/**
 * AnyUDF is the base class for all UDFs in carnot.
 */
class AnyUDF {};

/**
 * ScalarUDF is a wrapper around a stateless function that can take one more more UDF values
 * and return a single UDF value.
 *
 * In the lifetime of a query, one more more instances may be created. The implementation should
 * take care not to store local state that can change functionality from call to call (ie. The
 * Exec function should be pure).
 *
 * The derived class must implement:
 *      UDFValue Exec(FunctionContext *ctx, UDFValue... value) {}
 * This function is called for each record for which this UDF needs to execute.
 *
 * The ScalarUDF can _optionally_ implement the following function:
 *      Status Init(FunctionContext *ctx, UDFValue... init_args) {}
 *  This function is called once during initialization of each instance (many instances
 *  may exists in a given query). The arguments are as provided by the query.
 */
class ScalarUDF : AnyUDF {};

// SFINAE test for init fn.
// TODO(zasgar): We really want to also test the argument/return types.
template <typename T, typename = void>
struct has_udf_init_fn : std::false_type {};

template <typename T>
struct has_udf_init_fn<T, std::void_t<decltype(&T::Init)>> : std::true_type {};

/**
 * ScalarUDFTraits allows access to compile time traits of the class. For example,
 * argument types.
 * @tparam T A class that derives from ScalarUDF.
 */
template <typename T, typename = std::enable_if_t<std::is_base_of_v<ScalarUDF, T>>>
class ScalarUDFTraits {
 public:
  /**
   * Arguments types of Exec.
   * @return a vector of UDF data types.
   */
  static std::vector<UDFDataType> ExecArguments() { return GetArgumentTypesHelper(&T::Exec); }

  /**
   * Return types of the Exec function
   * @return A UDFDataType which is the return type of the Exec function.
   */
  static UDFDataType ReturnType() { return ReturnTypeHelper(&T::Exec); }

  /**
   * Checks if the UDF has an Init function.
   * @return true if it has an Init function.
   */
  static constexpr bool HasInit() { return has_udf_init_fn<T>::value; }

 private:
  template <typename ReturnType, typename... Types>
  static UDFDataType ReturnTypeHelper(ReturnType (T::*)(Types...)) {
    return UDFValueTraits<ReturnType>::data_type;
  }

  template <typename ReturnType, typename... Types>
  static std::vector<UDFDataType> GetArgumentTypesHelper(ReturnType (T::*)(FunctionContext*,
                                                                           Types...)) {
    return {UDFValueTraits<Types>::data_type...};
  }
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
