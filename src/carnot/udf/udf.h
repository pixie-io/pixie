#pragma once
#include <arrow/builder.h>
#include <arrow/type.h>
#include <glog/logging.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <functional>

#include "src/common/status.h"
#include "src/common/types/types.pb.h"

namespace pl {
namespace carnot {
namespace udf {

/**
 * The enum of different UDF data types.
 * TODO(zasgar): move this data type to a higher lvl than plan.
 */
using UDFDataType = types::DataType;

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
  // We need this to make UBSAN happy since otherwise it's un-initialized.
  // TODO(zasgar): Understand if this impacts performance.
  T val = 0;
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
struct StringValue : UDFBaseValue, public std::string {
  using std::string::string;
  // Allow implicit construction to make it easier/more natural to return values
  // from functions.
  // NOLINTNEXTLINE(runtime/explicit)
  StringValue(std::string&& str) : std::string(std::move(str)) {}
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
  static constexpr UDFDataType data_type = types::BOOLEAN;
  using arrow_type = arrow::BooleanType;
  using arrow_builder_type = arrow::BooleanBuilder;
  using arrow_array_type = arrow::BooleanArray;
  using native_type = bool;
};

template <>
struct UDFValueTraits<Int64Value> {
  static constexpr UDFDataType data_type = types::INT64;
  using arrow_type = arrow::Int64Type;
  using arrow_builder_type = arrow::Int64Builder;
  using arrow_array_type = arrow::Int64Array;
  using native_type = int64_t;
};

template <>
struct UDFValueTraits<Float64Value> {
  static constexpr UDFDataType data_type = types::FLOAT64;
  using arrow_type = arrow::DoubleType;
  using arrow_builder_type = arrow::DoubleBuilder;
  using arrow_array_type = arrow::DoubleArray;
  using native_type = double;
};

template <>
struct UDFValueTraits<StringValue> {
  static constexpr UDFDataType data_type = types::STRING;
  using arrow_type = arrow::StringType;
  using arrow_builder_type = arrow::StringBuilder;
  using arrow_array_type = arrow::StringArray;
  using native_type = std::string;
};

/**
 * Store traits based on the native UDF type.
 * @tparam T THe UDFDataType.
 */
template <udf::UDFDataType T>
struct UDFDataTypeTraits {};

template <>
struct UDFDataTypeTraits<udf::UDFDataType::BOOLEAN> {
  typedef BoolValue udf_value_type;
  using arrow_type = arrow::BooleanType;
  using arrow_builder_type = arrow::BooleanBuilder;
  using arrow_array_type = arrow::BooleanArray;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::BOOL;
};

template <>
struct UDFDataTypeTraits<udf::UDFDataType::INT64> {
  typedef Int64Value udf_value_type;
  using arrow_type = arrow::Int64Type;
  using arrow_builder_type = arrow::Int64Builder;
  using arrow_array_type = arrow::Int64Array;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::INT64;
};

template <>
struct UDFDataTypeTraits<udf::UDFDataType::FLOAT64> {
  typedef Float64Value udf_value_type;
  using arrow_type = arrow::DoubleType;
  using arrow_builder_type = arrow::DoubleBuilder;
  using arrow_array_type = arrow::DoubleArray;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::DOUBLE;
};

template <>
struct UDFDataTypeTraits<udf::UDFDataType::STRING> {
  typedef StringValue udf_value_type;
  using arrow_type = arrow::StringType;
  using arrow_builder_type = arrow::StringBuilder;
  using arrow_array_type = arrow::StringArray;
  static constexpr arrow::Type::type arrow_type_id = arrow::Type::STRING;
};

/**
 * Function context contains contextual resources such as mempools that functions
 * can use while executing.
 */
class FunctionContext {};

/**
 * AnyUDF is the base class for all UDFs in carnot.
 */
class AnyUDF {
 public:
  virtual ~AnyUDF() = default;
};

/**
 * Any UDA is a base class for all UDAs in carnot.
 */
class AnyUDA {
 public:
  virtual ~AnyUDA() = default;
};

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
class ScalarUDF : public AnyUDF {
 public:
  ~ScalarUDF() override = default;
};

/**
 * UDA is a stateful function that updates internal state bases on the input
 * values. It must be Merge-able with other UDAs of the same type.
 *
 * In the lifetime of the query one or more instances will be created. The Merge function
 * will be called to combine multiple instances together before destruction.
 *
 * The derived class must implement:
 *     void Update(FunctionContext *ctx, Args...) {}
 *     void Merge(FunctionContext *ctx, const SampleUDA& other) {}
 *     ReturnValue Finalize(FunctionContext *ctx) {}
 *
 * It may optionally implement:
 *     Status Init(FunctionContext *ctx, InitArgs...) {}
 *
 * All argument types must me valid UDFValueTypes.
 */
class UDA : public AnyUDA {
 public:
  ~UDA() override = default;
};

// SFINAE test for init fn.
// TODO(zasgar): We really want to also test the argument/return types.
template <typename T, typename = void>
struct has_udf_init_fn : std::false_type {};

template <typename T>
struct has_udf_init_fn<T, std::void_t<decltype(&T::Init)>> : std::true_type {
  static_assert(
      IsValidInitFn(&T::Init),
      "If an init functions exists it must have the form: Status Init(FunctionContext*, ...)");
};

/**
 * Checks to see if a valid looking Init Function exists.
 */
template <typename ReturnType, typename TUDF, typename... Types>
static constexpr bool IsValidInitFn(ReturnType (TUDF::*)(Types...)) {
  return false;
}

template <typename TUDF, typename... Types>
static constexpr bool IsValidInitFn(Status (TUDF::*)(FunctionContext*, Types...)) {
  return true;
}

/**
 * Checks to see if a valid looking Exec Function exists.
 */
template <typename ReturnType, typename TUDF, typename... Types>
static constexpr bool IsValidExecFunc(ReturnType (TUDF::*)(Types...)) {
  return false;
}

template <typename ReturnType, typename TUDF, typename... Types>
static constexpr bool IsValidExecFunc(ReturnType (TUDF::*)(FunctionContext*, Types...)) {
  return true;
}

template <typename ReturnType, typename TUDF, typename... Types>
static constexpr std::array<UDFDataType, sizeof...(Types)> GetArgumentTypesHelper(
    ReturnType (TUDF::*)(FunctionContext*, Types...)) {
  return std::array<UDFDataType, sizeof...(Types)>({UDFValueTraits<Types>::data_type...});
}

template <typename ReturnType, typename TUDF, typename... Types>
static constexpr UDFDataType ReturnTypeHelper(ReturnType (TUDF::*)(Types...)) {
  return UDFValueTraits<ReturnType>::data_type;
}

template <typename T, typename = void>
struct check_init_fn {};

template <typename T>
struct check_init_fn<T, typename std::enable_if_t<has_udf_init_fn<T>::value>> {
  static_assert(IsValidInitFn(&T::Init),
                "must have a valid Init fn, in form: Status Init(FunctionContext*, ...)");
};

/**
 * ScalarUDFTraits allows access to compile time traits of the class. For example,
 * argument types.
 * @tparam T A class that derives from ScalarUDF.
 */
template <typename T>
class ScalarUDFTraits {
 public:
  /**
   * Arguments types of Exec.
   * @return a vector of UDF data types.
   */
  static constexpr auto ExecArguments() { return GetArgumentTypesHelper(&T::Exec); }

  /**
   * Return types of the Exec function
   * @return A UDFDataType which is the return type of the Exec function.
   */
  static constexpr UDFDataType ReturnType() { return ReturnTypeHelper(&T::Exec); }

  /**
   * Checks if the UDF has an Init function.
   * @return true if it has an Init function.
   */
  static constexpr bool HasInit() { return has_udf_init_fn<T>::value; }

 private:
  struct check_valid_udf {
    static_assert(std::is_base_of_v<ScalarUDF, T>, "UDF must be derived from ScalarUDF");
    static_assert(IsValidExecFunc(&T::Exec),
                  "must have a valid Exec fn, in form: UDFValue Exec(FunctionContext*, ...)");

   private:
    static constexpr check_init_fn<T> check_init_;
  } check_;
};

/**
 * These are function type checkers for UDAs. Ideally these would all be pure
 * SFINAE templates, but the overload makes the code a bit easier to read.
 */

/**
 * Checks to see if a valid looking Update function exists.
 */
template <typename ReturnType, typename TUDA, typename... Types>
static constexpr bool IsValidUpdateFn(ReturnType (TUDA::*)(Types...)) {
  return false;
}

template <typename TUDA, typename... Types>
static constexpr bool IsValidUpdateFn(void (TUDA::*)(FunctionContext*, Types...)) {
  return true;
}

/**
 * Checks to see if a valid looking Merge Function exists.
 */
template <typename ReturnType, typename TUDA, typename... Types>
static constexpr bool IsValidMergeFn(ReturnType (TUDA::*)(Types...)) {
  return false;
}

template <typename TUDA>
static constexpr bool IsValidMergeFn(void (TUDA::*)(FunctionContext*, const TUDA&)) {
  return true;
}

/**
 * Checks to see if a valid looking Finalize Function exists.
 */
template <typename ReturnType, typename TUDA, typename... Types>
static constexpr bool IsValidFinalizeFn(ReturnType (TUDA::*)(Types...)) {
  return false;
}

template <typename ReturnType, typename TUDA, typename... Types>
static constexpr bool IsValidFinalizeFn(ReturnType (TUDA::*)(FunctionContext*)) {
  if (IsValidUDFDataType<ReturnType>::value) {
    return true;
  }
  return false;
}

/**
 * ScalarUDFTraits allows access to compile time traits of a given UDA.
 * @tparam T A class that derives from UDA.
 */
template <typename T>
class UDATraits {
 public:
  static constexpr auto UpdateArgumentTypes() { return GetArgumentTypesHelper<void>(&T::Update); }
  static constexpr UDFDataType FinalizeReturnType() { return ReturnTypeHelper(&T::Finalize); }

  /**
   * Checks if the UDA has an Init function.
   * @return true if it has an Init function.
   */
  static constexpr bool HasInit() { return has_udf_init_fn<T>::value; }

 private:
  /**
   * Static asserts to validate that the UDA is well formed.
   */
  struct check_valid_uda {
    static_assert(std::is_base_of_v<UDA, T>, "UDA must be derived from UDA");
    static_assert(IsValidUpdateFn(&T::Update),
                  "must have a valid Update fn, in form: void Update(FunctionContext*, ...)");
    static_assert(IsValidMergeFn(&T::Merge),
                  "must have a valid Merge fn, in form: void Merge(FunctionContext*, const UDA&)");
    static_assert(IsValidFinalizeFn(&T::Finalize),
                  "must have a valid Finalize fn, in form: ReturnType Finalize(FunctionContext*)");
    static constexpr check_init_fn<T> check_init_;
  } check_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
