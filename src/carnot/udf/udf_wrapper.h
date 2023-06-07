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

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/udf.h"
#include "src/carnot/udf/udtf.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace udf {

// This is the size we assume for an "average" string. If this is too large we waste
// memory, if too small we spend more time allocating. Since we use exponential doubling
// it's better to keep this number small.
const int kStringAssumedSizeHeuristic = 10;

// This function takes in a generic types::BaseValueType and then converts it to actual
// UDFValue type. This function is unsafe and will produce wrong results (or crash)
// if used incorrectly.
template <types::DataType TExecArgType>
constexpr auto CastToUDFValueType(const types::BaseValueType* arg) {
  // A sample transformation (for TExecArgType = types::DataType::INT64) is:
  // return static_cast<types::Int64Value*>(arg);
  return static_cast<const typename types::DataTypeTraits<TExecArgType>::value_type*>(arg);
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
Status ExecWrapper(TUDF* udf, FunctionContext* ctx, size_t count, TOutput* out,
                   const std::vector<const types::BaseValueType*>& args,
                   std::index_sequence<I...>) {
  [[maybe_unused]] constexpr auto exec_argument_types = ScalarUDFTraits<TUDF>::ExecArguments();
  for (size_t idx = 0; idx < count; ++idx) {
    out[idx] = udf->Exec(ctx, CastToUDFValueType<exec_argument_types[I]>(args[I])[idx]...);
  }
  return Status::OK();
}

template <typename TUDF, std::size_t... I>
Status InitWrapper(TUDF* udf, FunctionContext* ctx,
                   const std::vector<std::shared_ptr<types::BaseValueType>>& args,
                   std::index_sequence<I...>) {
  [[maybe_unused]] constexpr auto init_argument_types = ScalarUDFTraits<TUDF>::InitArguments();
  return udf->Init(ctx, *CastToUDFValueType<init_argument_types[I]>(args[I].get())...);
}

template <typename TUDA, std::size_t... I>
Status UDAInitWrapper(TUDA* udf, FunctionContext* ctx,
                      const std::vector<std::shared_ptr<types::BaseValueType>>& args,
                      std::index_sequence<I...>) {
  [[maybe_unused]] constexpr auto init_argument_types = UDATraits<TUDA>::InitArguments();
  return udf->Init(ctx, *CastToUDFValueType<init_argument_types[I]>(args[I].get())...);
}

// Returns the underlying data from a UDF value.
template <typename T>
inline auto UnWrap(const T& v) {
  return v.val;
}

template <>
inline auto UnWrap<types::StringValue>(const types::StringValue& s) {
  return s;
}

/**
 * This is the inner wrapper for the arrow type.
 * This performs type casting and storing the data in the output builder.
 */
template <typename TUDF, typename TOutput, std::size_t... I>
Status ExecWrapperArrow(TUDF* udf, FunctionContext* ctx, size_t count, TOutput* out,
                        const std::vector<arrow::Array*>& args, std::index_sequence<I...>) {
  [[maybe_unused]] static constexpr auto exec_argument_types =
      ScalarUDFTraits<TUDF>::ExecArguments();
  CHECK(out->Reserve(count).ok());
  size_t reserved = count * kStringAssumedSizeHeuristic;
  size_t total_size = 0;
  // If it's a string type we also need to allocate memory for the data.
  // This actually applies to all non-fixed data allocations.
  // PX_CARNOT_UPDATE_FOR_NEW_TYPES.
  if constexpr (std::is_same_v<arrow::StringBuilder, TOutput>) {
    CHECK(out->ReserveData(reserved).ok());
  }
  for (size_t idx = 0; idx < count; ++idx) {
    auto res = UnWrap(
        udf->Exec(ctx, types::GetValueFromArrowArray<exec_argument_types[I]>(args[I], idx)...));

    // We use doubling to make sure we minimize the number of allocations.
    // PX_CARNOT_UPDATE_FOR_NEW_TYPES.
    if constexpr (std::is_same_v<arrow::StringBuilder, TOutput>) {
      total_size += res.size();
      while (total_size >= reserved) {
        reserved *= 2;
        PX_RETURN_IF_ERROR(out->ReserveData(reserved));
      }
    }
    // This function is "safe" now because we manually allocated memory.
    out->UnsafeAppend(res);
  }
  return Status::OK();
}

/**
 * Checks types between column wrapper and array of types::UDFDataTypes.
 * @return true if all types match.
 */
template <std::size_t SIZE>
inline bool CheckTypes(const std::vector<const types::ColumnWrapper*>& args,
                       const std::array<types::DataType, SIZE>& types) {
  if (args.size() != SIZE) {
    return false;
  }
  for (size_t idx = 0; idx < args.size(); ++idx) {
    if (args[idx]->data_type() != types[idx]) {
      return false;
    }
  }
  return true;
}

inline std::vector<const types::BaseValueType*> ConvertToBaseValue(
    const std::vector<const types::ColumnWrapper*>& args) {
  std::vector<const types::BaseValueType*> retval;
  for (const auto* col : args) {
    retval.push_back(col->UnsafeRawData());
  }
  return retval;
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
  static Status ExecBatchArrow(ScalarUDF* udf, FunctionContext* ctx,
                               const std::vector<arrow::Array*>& inputs,
                               arrow::ArrayBuilder* output, int count) {
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
        static_cast<TUDF*>(udf), ctx, count,
        static_cast<typename types::DataTypeTraits<return_type>::arrow_builder_type*>(output),
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
  static Status ExecBatch(ScalarUDF* udf, FunctionContext* ctx,
                          const std::vector<const types::ColumnWrapper*>& inputs,
                          types::ColumnWrapper* output, int count) {
    // Check that output is allocated.
    DCHECK(output != nullptr);
    // Check that the arity is correct.
    DCHECK(inputs.size() == ScalarUDFTraits<TUDF>::ExecArguments().size());

    constexpr types::DataType return_type = ScalarUDFTraits<TUDF>::ReturnType();
    auto exec_argument_types = ScalarUDFTraits<TUDF>::ExecArguments();
    DCHECK(CheckTypes(inputs, exec_argument_types));
    auto input_as_base_value = ConvertToBaseValue(inputs);

    using output_type = typename types::DataTypeTraits<return_type>::value_type;
    auto* casted_output = static_cast<output_type*>(output->UnsafeRawData());
    // The outer wrapper just casts the output type and UDF type. We then pass in
    // the inputs with a sequence based on the number of arguments to iterate through and
    // cast the inputs.
    return ExecWrapper<TUDF>(static_cast<TUDF*>(udf), ctx, count, casted_output,
                             input_as_base_value,
                             std::make_index_sequence<exec_argument_types.size()>{});
  }

  /**
   * Call the UDF's init method.
   *
   * This function is unsafe and will assume the BaseValueType pointers are actually pointers to the
   * correct ValueType's.
   *
   * @param udf a pointer to the UDF
   * @param ctx The function context.
   * @param inputs An array of pointers to base value types to input into the init function.
   * @return Status from the udf's init function.
   */
  template <typename Q = TUDF, std::enable_if_t<ScalarUDFTraits<Q>::HasInit(), void>* = nullptr>
  static Status ExecInitImpl(ScalarUDF* udf, FunctionContext* ctx,
                             const std::vector<std::shared_ptr<types::BaseValueType>>& inputs) {
    auto init_argument_types = ScalarUDFTraits<Q>::InitArguments();
    return InitWrapper<Q>(static_cast<Q*>(udf), ctx, inputs,
                          std::make_index_sequence<init_argument_types.size()>{});
  }

  /**
   * Return Status::OK, if the UDF doesn't have an Init method.
   */
  template <typename Q = TUDF, std::enable_if_t<!ScalarUDFTraits<Q>::HasInit(), void>* = nullptr>
  static Status ExecInitImpl(ScalarUDF*, FunctionContext*,
                             const std::vector<std::shared_ptr<types::BaseValueType>>&) {
    return Status::OK();
  }

  static Status ExecInit(ScalarUDF* udf, FunctionContext* ctx,
                         const std::vector<std::shared_ptr<types::BaseValueType>>& inputs) {
    return ExecInitImpl(udf, ctx, inputs);
  }
};

/**
 * Performs an update on a batch of records.
 * This is similar to the ExecBatch, except it does not store a return value.
 */
template <typename TUDA, std::size_t... I>
Status UpdateWrapper(TUDA* uda, FunctionContext* ctx, size_t count,
                     const std::vector<const types::BaseValueType*>& args,
                     std::index_sequence<I...>) {
  constexpr auto update_argument_types = UDATraits<TUDA>::UpdateArgumentTypes();
  for (size_t idx = 0; idx < count; ++idx) {
    uda->Update(ctx, CastToUDFValueType<update_argument_types[I]>(args[I])[idx]...);
  }
  return Status::OK();
}

/**
 * Performs an update on a batch of records (arrow).
 * This is similar to the ExecBatch, except it does not store a return value.
 */
template <typename TUDA, std::size_t... I>
Status UpdateWrapperArrow(TUDA* uda, FunctionContext* ctx, size_t count,
                          const std::vector<const arrow::Array*>& args, std::index_sequence<I...>) {
  constexpr auto update_argument_types = UDATraits<TUDA>::UpdateArgumentTypes();
  for (size_t idx = 0; idx < count; ++idx) {
    uda->Update(ctx, types::GetValueFromArrowArray<update_argument_types[I]>(args[I], idx)...);
  }
  return Status::OK();
}

/**
 * Provides a set of static methods that wrap UDAs and allow vectorized execution (for update).
 * @tparam TUDA The UDA class.
 */
template <typename TUDA>
struct UDAWrapper {
  static constexpr types::DataType return_type = UDATraits<TUDA>::FinalizeReturnType();
  static constexpr bool SupportsPartial = UDATraits<TUDA>::SupportsPartial();

  /**
   * Create a new UDA.
   * @return A unique_ptr to the UDA instance.
   */
  static std::unique_ptr<UDA> Make() { return std::make_unique<TUDA>(); }

  /**
   * Perform a batch update of the passed in UDA based in the inputs.
   * @param uda The UDA instances.
   * @param ctx The function context.
   * @param inputs A vector of pointers to types::ColumnWrappers.
   * @return Status of update.
   */
  static Status ExecBatchUpdate(UDA* uda, FunctionContext* ctx,
                                const std::vector<const types::ColumnWrapper*>& inputs) {
    constexpr auto update_argument_types = UDATraits<TUDA>::UpdateArgumentTypes();
    DCHECK(CheckTypes(inputs, update_argument_types));
    DCHECK(inputs.size() == update_argument_types.size());

    auto input_as_base_value = ConvertToBaseValue(inputs);

    size_t num_records = inputs[0]->Size();
    return UpdateWrapper<TUDA>(static_cast<TUDA*>(uda), ctx, num_records, input_as_base_value,
                               std::make_index_sequence<update_argument_types.size()>{});
  }

  /**
   * Perform a batch update of the passed in UDA based in the inputs.
   * @param uda The UDA instances.
   * @param ctx The function context.
   * @param inputs A vector of pointers to types::ColumnWrappers.
   * @return Status of update.
   */
  static Status ExecBatchUpdateArrow(UDA* uda, FunctionContext* ctx,
                                     const std::vector<const arrow::Array*>& inputs) {
    constexpr auto update_argument_types = UDATraits<TUDA>::UpdateArgumentTypes();
    DCHECK(inputs.size() == update_argument_types.size());

    size_t num_records = inputs[0]->length();
    return UpdateWrapperArrow<TUDA>(static_cast<TUDA*>(uda), ctx, num_records, inputs,
                                    std::make_index_sequence<update_argument_types.size()>{});
  }

  /**
   * Call the UDA's init method.
   *
   * This function is unsafe and will assume the BaseValueType pointers are actually pointers to the
   * correct ValueType's.
   *
   * @param udf a pointer to the UDF
   * @param ctx The function context.
   * @param inputs An array of pointers to base value types to input into the init function.
   * @return Status from the uda's init function.
   */
  template <typename Q = TUDA, std::enable_if_t<UDATraits<Q>::HasInit(), void>* = nullptr>
  static Status ExecInitImpl(UDA* uda, FunctionContext* ctx,
                             const std::vector<std::shared_ptr<types::BaseValueType>>& inputs) {
    auto init_argument_types = UDATraits<TUDA>::InitArguments();
    return UDAInitWrapper<TUDA>(static_cast<TUDA*>(uda), ctx, inputs,
                                std::make_index_sequence<init_argument_types.size()>{});
  }

  /**
   * Return Status::OK, if the UDA doesn't have an Init method.
   */
  template <typename Q = TUDA, std::enable_if_t<!UDATraits<Q>::HasInit(), void>* = nullptr>
  static Status ExecInitImpl(UDA*, FunctionContext*,
                             const std::vector<std::shared_ptr<types::BaseValueType>>&) {
    return Status::OK();
  }

  static Status ExecInit(UDA* uda, FunctionContext* ctx,
                         const std::vector<std::shared_ptr<types::BaseValueType>>& inputs) {
    return ExecInitImpl(uda, ctx, inputs);
  }

  /**
   * Merges uda2 into uda1 based on the UDA merge function.
   * Both UDAs must be the same time, undefined behavior (or crash) if they are different types
   * or not the same type as the templated UDA.
   * @return Status of Merge.
   */
  static Status Merge(UDA* uda1, UDA* uda2, FunctionContext* ctx) {
    static_cast<TUDA*>(uda1)->Merge(ctx, *static_cast<TUDA*>(uda2));
    return Status::OK();
  }

  /**
   * Finalize the UDA into an arrow builder. The arrow builder needs to be correct type
   * for the finalize return type.
   * @return Status of the finalize.
   */
  static Status FinalizeArrow(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output) {
    DCHECK(output != nullptr);
    auto* casted_builder =
        static_cast<typename types::DataTypeTraits<return_type>::arrow_builder_type*>(output);
    auto* casted_uda = static_cast<TUDA*>(uda);
    PX_RETURN_IF_ERROR(casted_builder->Append(UnWrap(casted_uda->Finalize(ctx))));
    return Status::OK();
  }

  /**
   * Finalize the UDA into an UDA value type.
   * Unsafe casts are performed, so the passed in output UDA value must be of the correct
   * finalize return type.
   *
   * @return Status of the Finalize.
   */
  static Status FinalizeValue(UDA* uda, FunctionContext* ctx, types::BaseValueType* output) {
    using output_type = typename types::DataTypeTraits<return_type>::value_type;

    DCHECK(output != nullptr);
    auto* casted_output = static_cast<output_type*>(output);
    auto* casted_uda = static_cast<TUDA*>(uda);
    *casted_output = casted_uda->Finalize(ctx);
    return Status::OK();
  }

  /**
   * Call the UDA's Serialize method.
   *
   * @param uda a pointer to the UDA
   * @param ctx The function context.
   * @param output An arrow array builder to put the serialized output in.
   * @return Status from the uda's serialize function.
   */
  template <typename Q = TUDA, std::enable_if_t<UDATraits<Q>::SupportsPartial(), void>* = nullptr>
  static Status SerializeArrowImpl(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output) {
    auto* casted_builder = static_cast<arrow::StringBuilder*>(output);
    auto* casted_uda = static_cast<TUDA*>(uda);
    PX_RETURN_IF_ERROR(casted_builder->Append(UnWrap(casted_uda->Serialize(ctx))));
    return Status::OK();
  }

  /**
   * Return Status::OK, if the UDA doesn't have a Serialize method.
   */
  template <typename Q = TUDA, std::enable_if_t<!UDATraits<Q>::SupportsPartial(), void>* = nullptr>
  static Status SerializeArrowImpl(UDA*, FunctionContext*, arrow::ArrayBuilder*) {
    return Status::OK();
  }
  static Status SerializeArrow(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output) {
    return SerializeArrowImpl(uda, ctx, output);
  }
  /**
   * Call the UDA's Deserialize method.
   *
   * @param uda a pointer to the UDA
   * @param ctx The function context.
   * @param serialized A StringValue holding the data to deserialize.
   * @return Status from the uda's deserialize function.
   */
  template <typename Q = TUDA, std::enable_if_t<UDATraits<Q>::SupportsPartial(), void>* = nullptr>
  static Status DeserializeImpl(UDA* uda, FunctionContext* ctx,
                                const types::StringValue& serialized) {
    auto* casted_uda = static_cast<TUDA*>(uda);
    PX_RETURN_IF_ERROR(casted_uda->Deserialize(ctx, serialized));
    return Status::OK();
  }

  /**
   * Return Status::OK, if the UDA doesn't have a Deserialize method.
   */
  template <typename Q = TUDA, std::enable_if_t<!UDATraits<Q>::SupportsPartial(), void>* = nullptr>
  static Status DeserializeImpl(UDA*, FunctionContext*, const types::StringValue&) {
    return Status::OK();
  }
  static Status Deserialize(UDA* uda, FunctionContext* ctx, const types::StringValue& serialized) {
    return DeserializeImpl(uda, ctx, serialized);
  }
};

/**
 * UDTFWrapper provides and executable wrapper that vectorizes the UDTF code.
 * The results are written to the passed in arrow column builders.
 * @tparam TUDTF
 */
template <typename TUDTF>
struct UDTFWrapper {
  /**
   * Create a new UDTF.
   * @return A unique_ptr to the UDTF instance.
   */
  static std::unique_ptr<AnyUDTF> Make() { return std::make_unique<TUDTF>(); }

  static Status Init(AnyUDTF* udtf, FunctionContext* ctx,
                     const std::vector<const types::BaseValueType*>& args) {
    if constexpr (UDTFTraits<TUDTF>::HasInitFn()) {
      auto* u = static_cast<TUDTF*>(udtf);
      return InitExecWrapper(
          u, ctx, args, std::make_index_sequence<UDTFTraits<TUDTF>::InitArgumentTypes().size()>{});
    }
    if (args.size()) {
      // These are to make GCC happy.
      PX_UNUSED(udtf);
      PX_UNUSED(ctx);
      return error::InvalidArgument("Got args for UDTF '%s', that takes no init args, ignoring...",
                                    typeid(TUDTF).name());
    }
    return Status::OK();
  }

  static bool ExecBatchUpdate(AnyUDTF* udtf, FunctionContext* ctx, int max_gen_records,
                              std::vector<arrow::ArrayBuilder*>* outputs) {
    if (max_gen_records == 0) {
      return false;
    }

    // Reserve the output.
    for (auto* out : *outputs) {
      CHECK(out->Reserve(max_gen_records).ok());
    }

    auto* u = static_cast<TUDTF*>(udtf);
    int count = 0;
    bool more = true;
    RecordWriterProxy<TUDTF> rw(outputs);
    while (count < max_gen_records && more) {
      more = u->NextRecord(ctx, &rw);
      ++count;
    }
    return more;
  }

 private:
  template <std::size_t... I>
  static Status InitExecWrapper(TUDTF* udtf, FunctionContext* ctx,
                                const std::vector<const types::BaseValueType*>& args,
                                std::index_sequence<I...>) {
    [[maybe_unused]] constexpr auto init_argument_types = UDTFTraits<TUDTF>::InitArgumentTypes();
    return udtf->Init(ctx, *CastToUDFValueType<init_argument_types[I]>(args[I])...);
  }
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
