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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/udf/udf_wrapper.h"
#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"

namespace px {
namespace carnot {
namespace udf {

enum class UDFDefinitionKind { kScalarUDF = 1, kUDA, kUDTF };

/**
 * Store definition of a UDF. This is meant ot be stored in the UDF registry and
 * includes things like execution wrappers.
 */
class UDFDefinition {
 public:
  UDFDefinition() = delete;
  UDFDefinition(UDFDefinitionKind kind, std::string_view name)
      : kind_(kind), name_(std::string(name)) {}

  virtual ~UDFDefinition() = default;

  /**
   * @return The overload dependent arguments that the registry uses to resolves UDFs.
   */
  virtual const std::vector<types::DataType>& RegistryArgTypes() = 0;

  /**
   * Gets an unowned pointer to definition.
   * @return The UDF definition.
   */
  virtual UDFDefinition* GetDefinition() { return this; }

  UDFDefinitionKind kind() const { return kind_; }

  /**
   * Access internal variable name.
   * @return Returns the name of the UDF.
   */
  const std::string& name() const { return name_; }

 private:
  UDFDefinitionKind kind_;
  std::string name_;
};

/**
 * Store the information for a single ScalarUDF.
 * TODO(zasgar): Also needs to store information like exec ptrs, etc.
 */
class ScalarUDFDefinition : public UDFDefinition {
 public:
  ScalarUDFDefinition() = delete;
  ~ScalarUDFDefinition() override = default;

  explicit ScalarUDFDefinition(std::string_view name)
      : UDFDefinition(UDFDefinitionKind::kScalarUDF, name) {}

  /**
   * Init a UDF definition with the given name and type.
   *
   * @tparam TUDF the UDF class. Must be a ScalarUDF.
   * @param name The name of the UDF.
   * @return Status success/error.
   */
  template <typename TUDF>
  Status Init() {
    exec_return_type_ = ScalarUDFTraits<TUDF>::ReturnType();
    auto exec_arguments_array = ScalarUDFTraits<TUDF>::ExecArguments();
    exec_arguments_ = {begin(exec_arguments_array), end(exec_arguments_array)};
    exec_wrapper_fn_ = ScalarUDFWrapper<TUDF>::ExecBatch;
    exec_wrapper_arrow_fn_ = ScalarUDFWrapper<TUDF>::ExecBatchArrow;
    init_wrapper_fn_ = ScalarUDFWrapper<TUDF>::ExecInit;

    auto init_arguments_array = ScalarUDFTraits<TUDF>::InitArguments();
    init_arguments_ = {begin(init_arguments_array), end(init_arguments_array)};

    registry_arguments_ = init_arguments_;
    registry_arguments_.insert(registry_arguments_.end(), exec_arguments_.begin(),
                               exec_arguments_.end());

    make_fn_ = ScalarUDFWrapper<TUDF>::Make;

    if constexpr (ScalarUDFTraits<TUDF>::HasExecutor()) {
      executor_ = TUDF::Executor();
    } else {
      executor_ = udfspb::UDFSourceExecutor::UDF_ALL;
    }

    return Status::OK();
  }

  ScalarUDFDefinition* GetDefinition() override { return this; }

  std::unique_ptr<ScalarUDF> Make() { return make_fn_(); }

  Status ExecBatch(ScalarUDF* udf, FunctionContext* ctx,
                   const std::vector<const types::ColumnWrapper*>& inputs,
                   types::ColumnWrapper* output, int count) {
    return exec_wrapper_fn_(udf, ctx, inputs, output, count);
  }

  Status ExecBatchArrow(ScalarUDF* udf, FunctionContext* ctx,
                        const std::vector<arrow::Array*>& inputs, arrow::ArrayBuilder* output,
                        int count) {
    return exec_wrapper_arrow_fn_(udf, ctx, inputs, output, count);
  }

  Status ExecInit(ScalarUDF* udf, FunctionContext* ctx,
                  const std::vector<std::shared_ptr<types::BaseValueType>>& inputs) {
    return init_wrapper_fn_(udf, ctx, inputs);
  }

  /**
   * Access internal variable exec_return_type.
   * @return the stored return types of the exec function.
   */
  types::DataType exec_return_type() const { return exec_return_type_; }
  const std::vector<types::DataType>& exec_arguments() const { return exec_arguments_; }
  const std::vector<types::DataType>& init_arguments() const { return init_arguments_; }
  udfspb::UDFSourceExecutor executor() const { return executor_; }

  const std::vector<types::DataType>& RegistryArgTypes() override { return registry_arguments_; }
  size_t Arity() const { return exec_arguments_.size(); }
  const auto& exec_wrapper() const { return exec_wrapper_fn_; }

 private:
  std::vector<types::DataType> init_arguments_;
  std::vector<types::DataType> exec_arguments_;
  std::vector<types::DataType> registry_arguments_;
  types::DataType exec_return_type_;
  udfspb::UDFSourceExecutor executor_;
  std::function<std::unique_ptr<ScalarUDF>()> make_fn_;
  std::function<Status(ScalarUDF*, FunctionContext* ctx,
                       const std::vector<const types::ColumnWrapper*>& inputs,
                       types::ColumnWrapper* output, int count)>
      exec_wrapper_fn_;

  std::function<Status(ScalarUDF* udf, FunctionContext* ctx,
                       const std::vector<arrow::Array*>& inputs, arrow::ArrayBuilder* output,
                       int count)>
      exec_wrapper_arrow_fn_;

  std::function<Status(ScalarUDF* udf, FunctionContext* ctx,
                       const std::vector<std::shared_ptr<types::BaseValueType>>& inputs)>
      init_wrapper_fn_;
};

/**
 * Store the information for a single UDA.
 */
class UDADefinition : public UDFDefinition {
 public:
  UDADefinition() = delete;
  ~UDADefinition() override = default;

  explicit UDADefinition(std::string_view name) : UDFDefinition(UDFDefinitionKind::kUDA, name) {}

  /**
   * Init a UDA definition with the given name and type.
   *
   * @tparam T the UDA class. Must be derived from UDA.
   * @param name The name of the UDA.
   * @return Status success/error.
   */
  template <typename T>
  Status Init() {
    auto update_arguments_array = UDATraits<T>::UpdateArgumentTypes();
    update_arguments_ = {update_arguments_array.begin(), update_arguments_array.end()};
    finalize_return_type_ = UDATraits<T>::FinalizeReturnType();
    make_fn_ = UDAWrapper<T>::Make;
    exec_batch_update_fn_ = UDAWrapper<T>::ExecBatchUpdate;
    exec_batch_update_arrow_fn_ = UDAWrapper<T>::ExecBatchUpdateArrow;
    init_wrapper_fn_ = UDAWrapper<T>::ExecInit;

    auto init_arguments_array = UDATraits<T>::InitArguments();
    init_arguments_ = {begin(init_arguments_array), end(init_arguments_array)};
    registry_arguments_ = init_arguments_;
    registry_arguments_.insert(registry_arguments_.end(), update_arguments_.begin(),
                               update_arguments_.end());

    merge_fn_ = UDAWrapper<T>::Merge;
    finalize_arrow_fn_ = UDAWrapper<T>::FinalizeArrow;
    finalize_value_fn = UDAWrapper<T>::FinalizeValue;

    supports_partial_ = UDAWrapper<T>::SupportsPartial;

    deserialize_fn_ = UDAWrapper<T>::Deserialize;
    serialize_arrow_fn_ = UDAWrapper<T>::SerializeArrow;
    return Status::OK();
  }

  UDADefinition* GetDefinition() override { return this; }

  const std::vector<types::DataType>& RegistryArgTypes() override { return registry_arguments_; }

  const std::vector<types::DataType>& update_arguments() const { return update_arguments_; }
  const std::vector<types::DataType>& init_arguments() const { return init_arguments_; }
  types::DataType finalize_return_type() const { return finalize_return_type_; }

  bool supports_partial() const { return supports_partial_; }

  std::unique_ptr<UDA> Make() { return make_fn_(); }

  Status ExecBatchUpdate(UDA* uda, FunctionContext* ctx,
                         const std::vector<const types::ColumnWrapper*>& inputs) {
    return exec_batch_update_fn_(uda, ctx, inputs);
  }
  Status ExecBatchUpdateArrow(UDA* uda, FunctionContext* ctx,
                              const std::vector<const arrow::Array*>& inputs) {
    return exec_batch_update_arrow_fn_(uda, ctx, inputs);
  }

  Status ExecInit(UDA* uda, FunctionContext* ctx,
                  const std::vector<std::shared_ptr<types::BaseValueType>>& inputs) {
    return init_wrapper_fn_(uda, ctx, inputs);
  }

  Status Merge(UDA* uda1, UDA* uda2, FunctionContext* ctx) { return merge_fn_(uda1, uda2, ctx); }
  Status FinalizeValue(UDA* uda, FunctionContext* ctx, types::BaseValueType* output) {
    return finalize_value_fn(uda, ctx, output);
  }
  Status FinalizeArrow(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output) {
    return finalize_arrow_fn_(uda, ctx, output);
  }
  Status Deserialize(UDA* uda, FunctionContext* ctx, const types::StringValue& serialized) {
    return deserialize_fn_(uda, ctx, serialized);
  }
  Status SerializeArrow(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output) {
    return serialize_arrow_fn_(uda, ctx, output);
  }

 private:
  std::vector<types::DataType> init_arguments_;
  std::vector<types::DataType> update_arguments_;
  std::vector<types::DataType> registry_arguments_;
  types::DataType finalize_return_type_;
  bool supports_partial_;

  std::function<std::unique_ptr<UDA>()> make_fn_;
  std::function<Status(UDA* uda, FunctionContext* ctx,
                       const std::vector<const types::ColumnWrapper*>& inputs)>
      exec_batch_update_fn_;

  std::function<Status(UDA* uda, FunctionContext* ctx,
                       const std::vector<const arrow::Array*>& inputs)>
      exec_batch_update_arrow_fn_;

  std::function<Status(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output)>
      finalize_arrow_fn_;
  std::function<Status(UDA* uda, FunctionContext* ctx, types::BaseValueType* output)>
      finalize_value_fn;
  std::function<Status(UDA* uda1, UDA* uda2, FunctionContext* ctx)> merge_fn_;
  std::function<Status(UDA* uda, FunctionContext* ctx,
                       const std::vector<std::shared_ptr<types::BaseValueType>>& inputs)>
      init_wrapper_fn_;
  std::function<Status(UDA* uda, FunctionContext* ctx, const types::StringValue& serialized)>
      deserialize_fn_;
  std::function<Status(UDA* uda, FunctionContext* ctx, arrow::ArrayBuilder* output)>
      serialize_arrow_fn_;
};

class UDTFDefinition : public UDFDefinition {
 public:
  UDTFDefinition() = delete;
  ~UDTFDefinition() override = default;

  explicit UDTFDefinition(std::string_view name) : UDFDefinition(UDFDefinitionKind::kUDTF, name) {}

  /**
   * Init a UDTF definition with the given name and type.
   *
   * @tparam T the UDTF class. Must be derived from UDTF.
   * @param name The name of the UDTF.
   * @return Status success/error.
   */
  template <typename T>
  Status Init() {
    auto factory = std::make_unique<GenericUDTFFactory<T>>();
    return Init<T>(std::move(factory));
  }

  /**
   * Init a UDTF definition with the given factory function.
   * @tparam T The UDTF def.
   * @param factory The UDTF factory.
   * @param name The name of the UDTF.
   * @return Status
   */
  template <typename T>
  Status Init(std::unique_ptr<UDTFFactory> factory) {
    factory_ = std::move(factory);
    // Check to make sure it's a valid UDTF.
    UDTFChecker<T> checker;
    PX_UNUSED(checker);

    exec_init_ = UDTFWrapper<T>::Init;
    exec_batch_update_ = UDTFWrapper<T>::ExecBatchUpdate;

    auto init_args = UDTFTraits<T>::InitArguments();
    init_arguments_ = {init_args.begin(), init_args.end()};

    auto output_relation = UDTFTraits<T>::OutputRelation();
    output_relation_ = {output_relation.begin(), output_relation.end()};

    executor_ = UDTFTraits<T>::Executor();

    return Status::OK();
  }

  const std::vector<types::DataType>& RegistryArgTypes() override {
    // UDTF's can't be overloaded.
    return args_types;
  }

  UDTFDefinition* GetDefinition() override { return this; }

  std::unique_ptr<AnyUDTF> Make() { return factory_->Make(); }

  Status ExecInit(AnyUDTF* udtf, FunctionContext* ctx,
                  const std::vector<const types::BaseValueType*>& args) {
    return exec_init_(udtf, ctx, args);
  }

  bool ExecBatchUpdate(AnyUDTF* udtf, FunctionContext* ctx, int max_gen_records,
                       std::vector<arrow::ArrayBuilder*>* outputs) {
    return exec_batch_update_(udtf, ctx, max_gen_records, outputs);
  }

  const std::vector<UDTFArg>& init_arguments() const { return init_arguments_; }
  const std::vector<ColInfo>& output_relation() const { return output_relation_; }
  udfspb::UDTFSourceExecutor executor() const { return executor_; }

 private:
  std::unique_ptr<UDTFFactory> factory_;
  std::function<Status(AnyUDTF*, FunctionContext*, const std::vector<const types::BaseValueType*>&)>
      exec_init_;
  std::function<bool(AnyUDTF* udtf, FunctionContext* ctx, int max_gen_records,
                     std::vector<arrow::ArrayBuilder*>* outputs)>
      exec_batch_update_;
  std::vector<UDTFArg> init_arguments_;
  std::vector<ColInfo> output_relation_;
  udfspb::UDTFSourceExecutor executor_;
  const std::vector<types::DataType>
      args_types{};  // Empty arg types because UDTF's can't be overloaded.
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
