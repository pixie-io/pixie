#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/column_wrapper.h"
#include "src/carnot/udf/udf_wrapper.h"
#include "src/utils/status.h"

namespace pl {
namespace carnot {
namespace udf {

/**
 * Store definition of a UDF. This is meant ot be stored in the UDF registry and
 * includes things like execution wrappers.
 */
class UDFDefinition {
 public:
  UDFDefinition() = default;
  virtual ~UDFDefinition() = default;

  Status Init(const std::string& name) {
    name_ = name;
    return Status::OK();
  }
  /**
   * @return The overload dependent arguments that the registry uses to resolves UDFs.
   */
  virtual const std::vector<UDFDataType>& RegistryArgTypes() = 0;

  /**
   * Gets an unowned pointer to definition.
   * @return The UDF definition.
   */
  virtual UDFDefinition* GetDefinition() { return this; }

  /**
   * Access internal variable name.
   * @return Returns the name of the UDF.
   */
  std::string name() { return name_; }

 private:
  std::string name_;
};

/**
 * Store the information for a single ScalarUDF.
 * TODO(zasgar): Also needs to store information like exec ptrs, etc.
 */
class ScalarUDFDefinition : public UDFDefinition {
 public:
  ScalarUDFDefinition() = default;
  ~ScalarUDFDefinition() override = default;

  /**
   * Init a UDF definition with the given name and type.
   *
   * @tparam TUDF the UDF class. Must be a ScalarUDF.
   * @param name The name of the UDF.
   * @return Status success/error.
   */
  template <typename TUDF>
  Status Init(const std::string& name) {
    PL_RETURN_IF_ERROR(UDFDefinition::Init(name));
    exec_return_type_ = ScalarUDFTraits<TUDF>::ReturnType();
    auto exec_arguments_array = ScalarUDFTraits<TUDF>::ExecArguments();
    exec_arguments_ = {begin(exec_arguments_array), end(exec_arguments_array)};
    exec_wrapper_fn_ = ScalarUDFWrapper<TUDF>::ExecBatch;
    exec_wrapper_arrow_fn_ = ScalarUDFWrapper<TUDF>::ExecBatchArrow;

    make_fn_ = ScalarUDFWrapper<TUDF>::Make;

    return Status::OK();
  }

  ScalarUDFDefinition* GetDefinition() override { return this; }

  std::unique_ptr<ScalarUDF> Make() { return make_fn_(); }

  Status ExecBatch(ScalarUDF* udf, FunctionContext* ctx,
                   const std::vector<const ColumnWrapper*>& inputs, ColumnWrapper* output,
                   int count) {
    return exec_wrapper_fn_(udf, ctx, inputs, output, count);
  }

  Status ExecBatchArrow(ScalarUDF* udf, FunctionContext* ctx,
                        const std::vector<arrow::Array*>& inputs, arrow::ArrayBuilder* output,
                        int count) {
    return exec_wrapper_arrow_fn_(udf, ctx, inputs, output, count);
  }

  /**
   * Access internal variable exec_return_type.
   * @return the stored return types of the exec function.
   */
  UDFDataType exec_return_type() const { return exec_return_type_; }
  const std::vector<UDFDataType>& exec_arguments() const { return exec_arguments_; }

  const std::vector<UDFDataType>& RegistryArgTypes() override { return exec_arguments_; }
  size_t Arity() const { return exec_arguments_.size(); }
  const auto& exec_wrapper() const { return exec_wrapper_fn_; }

 private:
  std::vector<UDFDataType> exec_arguments_;
  UDFDataType exec_return_type_;
  std::function<std::unique_ptr<ScalarUDF>()> make_fn_;
  std::function<Status(ScalarUDF*, FunctionContext* ctx,
                       const std::vector<const ColumnWrapper*>& inputs, ColumnWrapper* output,
                       int count)>
      exec_wrapper_fn_;

  std::function<Status(ScalarUDF* udf, FunctionContext* ctx,
                       const std::vector<arrow::Array*>& inputs, arrow::ArrayBuilder* output,
                       int count)>
      exec_wrapper_arrow_fn_;
};

/**
 * Store the information for a single UDA.
 * TODO(zasgar): Also, needs to store ptrs to exec funcs.
 */
class UDADefinition : public UDFDefinition {
 public:
  UDADefinition() = default;
  ~UDADefinition() override = default;
  /**
   * Init a UDA definition with the given name and type.
   *
   * @tparam T the UDA class. Must be derived from UDA.
   * @param name The name of the UDA.
   * @return Status success/error.
   */
  template <typename T>
  Status Init(const std::string& name) {
    PL_RETURN_IF_ERROR(UDFDefinition::Init(name));
    auto update_arguments_array = UDATraits<T>::UpdateArgumentTypes();
    update_arguments_ = {update_arguments_array.begin(), update_arguments_array.end()};
    finalize_return_type_ = UDATraits<T>::FinalizeReturnType();
    return Status::OK();
  }

  UDADefinition* GetDefinition() override { return this; }

  const std::vector<UDFDataType>& RegistryArgTypes() override { return update_arguments_; }

  const std::vector<UDFDataType> update_arguments() { return update_arguments_; }
  UDFDataType finalize_return_type() const { return finalize_return_type_; }

 private:
  std::vector<UDFDataType> update_arguments_;
  UDFDataType finalize_return_type_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
