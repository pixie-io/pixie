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

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <functional>

#include "src/carnot/udf/base.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace udf {

/**
 * AnyUDF is the base class for all UDFs in carnot.
 */
class AnyUDF : public BaseFunc {
 public:
  virtual ~AnyUDF() = default;
};

/**
 * Any UDA is a base class for all UDAs in carnot.
 */
class AnyUDA : public BaseFunc {
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
 * To support partial aggregation to UDAs must also implement:
 *     StringValue Serialize(FunctionContext*) {}
 *     Status DeSerialize(FunctionContext*, const StringValue& data) {}
 *
 * All argument types must me valid UDFValueTypes.
 */
class UDA : public AnyUDA {
 public:
  ~UDA() override = default;
};

// SFINAE test for init fn.
template <typename T, typename = void>
struct has_udf_init_fn : std::false_type {};

template <typename T>
struct has_udf_init_fn<T, std::void_t<decltype(&T::Init)>> : std::true_type {
  static_assert(
      IsValidInitFn(&T::Init),
      "If an init function exists, it must have the form: Status Init(FunctionContext*, ...)");
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

/**
 * Checks to see if a valid looking Executor function exists.
 */
template <typename ReturnType>
constexpr bool IsValidExecutorFn(ReturnType (*)()) {
  return false;
}

template <>
constexpr bool IsValidExecutorFn(udfspb::UDFSourceExecutor (*)()) {
  return true;
}

// SFINAE test for Executor fn.
template <typename T, typename = void>
struct has_udf_executor_fn : std::false_type {};

template <typename T>
struct has_udf_executor_fn<T, std::void_t<decltype(&T::Executor)>> : std::true_type {
  static_assert(
      IsValidExecutorFn(&T::Executor),
      "If an executor function exists, it must have the form: UDFSourceExecutor Executor()");
};

template <typename T, typename = void>
struct check_executor_fn {};

template <typename T>
struct check_executor_fn<T, typename std::enable_if_t<has_udf_executor_fn<T>::value>> {
  static_assert(IsValidExecutorFn(&T::Executor),
                "must have a valid Executor fn, in form: UDFSourceExecutor Executor()");
};

template <typename ReturnType, typename TUDF, typename... Types>
static constexpr std::array<types::DataType, sizeof...(Types)> GetArgumentTypesHelper(
    ReturnType (TUDF::*)(FunctionContext*, Types...)) {
  return std::array<types::DataType, sizeof...(Types)>(
      {types::ValueTypeTraits<Types>::data_type...});
}

template <typename ReturnType, typename TUDF, typename... Types>
static constexpr types::DataType ReturnTypeHelper(ReturnType (TUDF::*)(Types...)) {
  return types::ValueTypeTraits<ReturnType>::data_type;
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
   * @return A types::UDFDataType which is the return type of the Exec function.
   */
  static constexpr types::DataType ReturnType() { return ReturnTypeHelper(&T::Exec); }

  /**
   * Checks if the UDF has an Init function.
   * @return true if it has an Init function.
   */
  static constexpr bool HasInit() { return has_udf_init_fn<T>::value; }

  /**
   * Returns the executor type of this UDF.
   */
  static constexpr bool HasExecutor() { return has_udf_executor_fn<T>::value; }

  template <typename Q = T, std::enable_if_t<ScalarUDFTraits<Q>::HasInit(), void>* = nullptr>
  static constexpr auto InitArguments() {
    return GetArgumentTypesHelper(&Q::Init);
  }

  template <typename Q = T, std::enable_if_t<!ScalarUDFTraits<Q>::HasInit(), void>* = nullptr>
  static constexpr auto InitArguments() {
    return std::array<types::DataType, 0>{};
  }

 private:
  struct check_valid_udf {
    static_assert(std::is_base_of_v<ScalarUDF, T>, "UDF must be derived from ScalarUDF");
    static_assert(IsValidExecFunc(&T::Exec),
                  "must have a valid Exec fn, in form: UDFValue Exec(FunctionContext*, ...)");

   private:
    static constexpr check_init_fn<T> check_init_{};
    static constexpr check_executor_fn<T> check_executor_{};
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
  return static_cast<bool>(types::IsValidValueType<ReturnType>::value);
}

/**
 * Checks to see if a valid looking Serialize Function exists.
 */
template <typename ReturnType, typename TUDA, typename... Types>
static constexpr bool IsValidSerializeFn(ReturnType (TUDA::*)(Types...)) {
  return false;
}

template <typename TUDA>
static constexpr bool IsValidSerializeFn(types::StringValue (TUDA::*)(FunctionContext*)) {
  return true;
}

/**
 * Checks to see if a valid looking Deserialize Function exists.
 */
template <typename ReturnType, typename TUDA, typename... Types>
static constexpr bool IsValidDeserializeFn(ReturnType (TUDA::*)(Types...)) {
  return false;
}

template <typename TUDA>
static constexpr bool IsValidDeserializeFn(Status (TUDA::*)(FunctionContext*,
                                                            const types::StringValue&)) {
  return true;
}

// SFINAE test for serialize fn.
template <typename T, typename = void>
struct has_uda_serialize_fn : std::false_type {};

template <typename T>
struct has_uda_serialize_fn<T, std::void_t<decltype(&T::Serialize)>> : std::true_type {
  static_assert(IsValidSerializeFn(&T::Serialize),
                "If a serialize function exists it must have the form: StringValue "
                "Serialize(FunctionContext*)");
};

// SFINAE test for deserialize fn.
template <typename T, typename = void>
struct has_uda_deserialize_fn : std::false_type {};

template <typename T>
struct has_uda_deserialize_fn<T, std::void_t<decltype(&T::Deserialize)>> : std::true_type {
  static_assert(IsValidDeserializeFn(&T::Deserialize),
                "If an Deseriazlie functions exists it must have the form: Status "
                "Deserialize(FunctionContext*, const StringValue&)");
};

/**
 * ScalarUDFTraits allows access to compile time traits of a given UDA.
 * @tparam T A class that derives from UDA.
 */
template <typename T>
class UDATraits {
 public:
  static constexpr auto UpdateArgumentTypes() { return GetArgumentTypesHelper<void>(&T::Update); }
  static constexpr types::DataType FinalizeReturnType() { return ReturnTypeHelper(&T::Finalize); }

  /**
   * Checks if the UDA has an Init function.
   * @return true if it has an Init function.
   */
  static constexpr bool HasInit() { return has_udf_init_fn<T>::value; }

  /**
   * @brief Whether this UDA supports a partial aggregate representation
   * @return true
   * @return false
   */
  static constexpr bool SupportsPartial() {
    return has_uda_serialize_fn<T>() && has_uda_deserialize_fn<T>();
  }

  template <typename Q = T, std::enable_if_t<UDATraits<Q>::HasInit(), void>* = nullptr>
  static constexpr auto InitArguments() {
    return GetArgumentTypesHelper(&Q::Init);
  }

  template <typename Q = T, std::enable_if_t<!UDATraits<Q>::HasInit(), void>* = nullptr>
  static constexpr auto InitArguments() {
    return std::array<types::DataType, 0>{};
  }

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
    static constexpr check_init_fn<T> check_init_{};
  } check_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
