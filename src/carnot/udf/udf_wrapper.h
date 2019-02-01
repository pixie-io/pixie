#pragma once

#include <arrow/array.h>
#include <glog/logging.h>

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/arrow_adapter.h"
#include "src/carnot/udf/column_wrapper.h"
#include "src/carnot/udf/udf.h"
#include "src/common/error.h"
#include "src/common/statusor.h"

namespace pl {
namespace carnot {
namespace udf {

// This is the size we assume for an "average" string. If this is too large we waste
// memory, if too small we spend more time allocating. Since we use exponential doubling
// it's better to keep this number small.
const int kStringAssumedSizeHeuristic = 10;

// This function takes in a generic UDFBaseValue and then converts it to actual
// UDFValue type. This function is unsafe and will produce wrong results (or crash)
// if used incorrectly.
template <udf::UDFDataType TExecArgType>
constexpr auto CastToUDFValueType(const UDFBaseValue *arg) {
  // A sample transformation (for TExecArgType = UDFDataType::INT64) is:
  // return reinterpret_cast<Int64Value*>(arg);
  return reinterpret_cast<const typename UDFDataTypeTraits<TExecArgType>::udf_value_type *>(arg);
}
/**
 * This is the inner wrapper which expands the arguments an performs type casts
 * based on the type and arity of the input arguments.
 *
 * This function takes calls the Exec function of the UDF after type casting all the
 * input values. The function is called once for each row of the input batch.
 *
 * @return Status of execution.
 */
template <typename TUDF, typename TOutput, std::size_t... I>
Status ExecWrapper(TUDF *udf, FunctionContext *ctx, size_t count, TOutput *out,
                   const std::vector<const UDFBaseValue *> &args, std::index_sequence<I...>) {
  constexpr auto exec_argument_types = ScalarUDFTraits<TUDF>::ExecArguments();
  for (size_t idx = 0; idx < count; ++idx) {
    out[idx] = udf->Exec(ctx, CastToUDFValueType<exec_argument_types[I]>(args[I])[idx]...);
  }
  return Status::OK();
}

// The get value functions pluck out value at a specific index.
template <typename T>
inline auto GetValue(const T *arr, int64_t idx) {
  return arr->Value(idx);
}

// Specialization for string type.
template <>
inline auto GetValue<arrow::StringArray>(const arrow::StringArray *arr, int64_t idx) {
  return arr->GetString(idx);
}

// Returns the underlying data from a UDF value.
template <typename T>
inline auto UnWrap(const T &v) {
  return v.val;
}

template <>
inline auto UnWrap<StringValue>(const StringValue &s) {
  return s;
}

// This function takes in a generic arrow::Array and then converts it to actual
// specific arrow::Array subtype. This function is unsafe and will produce wrong results (or crash)
// if used incorrectly.
template <udf::UDFDataType TExecArgType>
constexpr auto GetValueFromArrowArray(const arrow::Array *arg, int64_t idx) {
  // A sample transformation (for TExecArgType = UDFDataType::INT64) is:
  // return GetValue(reinterpret_cast<arrow::Int64Array*>(arg), idx);
  using arrow_array_type = typename UDFDataTypeTraits<TExecArgType>::arrow_array_type;
  return GetValue(reinterpret_cast<const arrow_array_type *>(arg), idx);
}

/**
 * This is the inner wrapper for the arrow type.
 * This performs type casting and storing the data in the output builder.
 */
template <typename TUDF, typename TOutput, std::size_t... I>
Status ExecWrapperArrow(TUDF *udf, FunctionContext *ctx, size_t count, TOutput *out,
                        const std::vector<arrow::Array *> &args, std::index_sequence<I...>) {
  constexpr auto exec_argument_types = ScalarUDFTraits<TUDF>::ExecArguments();
  CHECK(out->Reserve(count).ok());
  size_t reserved = count * kStringAssumedSizeHeuristic;
  size_t total_size = 0;
  // If it's a string type we also need to allocate memory for the data.
  // This actually applies to all non-fixed data allocations.
  // PL_CARNOT_UPDATE_FOR_NEW_TYPES.
  if constexpr (std::is_same_v<arrow::StringBuilder, TOutput>) {
    CHECK(out->ReserveData(reserved).ok());
  }
  for (size_t idx = 0; idx < count; ++idx) {
    auto res =
        UnWrap(udf->Exec(ctx, GetValueFromArrowArray<exec_argument_types[I]>(args[I], idx)...));

    // We use doubling to make sure we minimize the number of allocations.
    // PL_CARNOT_UPDATE_FOR_NEW_TYPES.
    if constexpr (std::is_same_v<arrow::StringBuilder, TOutput>) {
      total_size += res.size();
      while (total_size >= reserved) {
        reserved *= 2;
        PL_RETURN_IF_ERROR(out->ReserveData(reserved));
      }
    }
    // This function is "safe" now because we manually allocated memory.
    out->UnsafeAppend(res);
  }
  return Status::OK();
}

/**
 * This struct is used to provide a set of static methods that wrap the init, and
 * exec methods of ScalarUDFs.
 *
 * @tparam TUDF A Scalar UDF.
 */
template <typename TUDF>
struct ScalarUDFWrapper {
  static std::unique_ptr<ScalarUDF> Make() { return std::make_unique<TUDF>(); }

  /**
   * Provides a method that executes the tempalated UDF on a batch of inputs.
   * The input batches are represented as vector of arrow:array pointers.
   *
   * This expects all the inputs and outputs to be allocated and of the appropriate
   * type. This function is unsafe and will perform unsafe casts and using an incorrect
   * type will result in a crash!
   *
   * @note This function and underlying templates are fully expanded at compile time.
   *
   * @param udf a pointer to the UDF.
   * @param ctx The function context.
   * @param inputs A vector of arrow::array* of inputs to the udf.
   * @param output The output builder.
   * @param count The number of elements in the input and out (these need to be the same).
   * @return Status of execution.
   */
  static Status ExecBatchArrow(ScalarUDF *udf, FunctionContext *ctx,
                               const std::vector<arrow::Array *> &inputs,
                               arrow::ArrayBuilder *output, int count) {
    constexpr types::DataType return_type = ScalarUDFTraits<TUDF>::ReturnType();
    auto exec_argument_types = ScalarUDFTraits<TUDF>::ExecArguments();

    // Check that output is allocated.
    DCHECK(output != nullptr);
    // Check that the arity is correct.
    DCHECK(inputs.size() == ScalarUDFTraits<TUDF>::ExecArguments().size());

    // The outer wrapper just casts the output type and UDF type. We then pass in
    // the inputs with a sequence based on the number of arguments to iterate through and
    // cast the inputs.
    return ExecWrapperArrow<TUDF>(
        reinterpret_cast<TUDF *>(udf), ctx, count,
        reinterpret_cast<typename UDFDataTypeTraits<return_type>::arrow_builder_type *>(output),
        inputs, std::make_index_sequence<exec_argument_types.size()>{});
  }

  /**
   * Provides a method that executes the tempalated UDF on a batch of inputs.
   * The input batches are represented as vector of vectors to the inputs.
   *
   * This expects all the inputs and outputs to be allocated and of the appropriate
   * type. This function is unsafe and will perform unsafe casts and using an incorrect
   * type will result in a crash!
   *
   * @note This function and underlying templates are fully expanded at compile time.
   *
   * @param udf a pointer to the UDF.
   * @param ctx The function context.
   * @param inputs An array of inputs to the udf.
   * @param output Pointer to the start of the output.
   * @param count The number of elements in the input and out (these need to be the same).
   * @return Status of execution.
   */
  static Status ExecBatch(ScalarUDF *udf, FunctionContext *ctx,
                          const std::vector<const ColumnWrapper *> &inputs, ColumnWrapper *output,
                          int count) {
    // Check that output is allocated.
    DCHECK(output != nullptr);
    // Check that the arity is correct.
    DCHECK(inputs.size() == ScalarUDFTraits<TUDF>::ExecArguments().size());

    constexpr types::DataType return_type = ScalarUDFTraits<TUDF>::ReturnType();
    auto exec_argument_types = ScalarUDFTraits<TUDF>::ExecArguments();

#ifndef NDEBUG
    // Check argument types in debug mode.
    for (size_t idx = 0; idx < inputs.size(); ++idx) {
      CHECK(inputs[idx]->DataType() == exec_argument_types[idx]);
    }
#endif

    std::vector<const UDFBaseValue *> input_as_base_value;
    for (const auto *col : inputs) {
      input_as_base_value.push_back(col->UnsafeRawData());
    }

    using output_type = typename UDFDataTypeTraits<return_type>::udf_value_type;
    auto *casted_output = reinterpret_cast<output_type *>(output->UnsafeRawData());
    // The outer wrapper just casts the output type and UDF type. We then pass in
    // the inputs with a sequence based on the number of arguments to iterate through and
    // cast the inputs.
    return ExecWrapper<TUDF>(reinterpret_cast<TUDF *>(udf), ctx, count, casted_output,
                             input_as_base_value,
                             std::make_index_sequence<exec_argument_types.size()>{});
  }
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
