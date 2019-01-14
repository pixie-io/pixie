#pragma once

#include <glog/logging.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/udf/udf.h"
#include "src/utils/error.h"
#include "src/utils/statusor.h"

namespace pl {
namespace carnot {
namespace udf {

/**
 * Store the information for a single ScalarUDF.
 * TODO(zasgar): Also needs to store information like exec ptrs, etc.
 */
class ScalarUDFDefinition {
 public:
  ScalarUDFDefinition() = default;

  /**
   * Init a UDF definition with the given name and type.
   *
   * @tparam T the UDF class. Must be a ScalarUDF.
   * @param name The name of the UDF.
   * @return Status success/error.
   */
  template <typename T>
  Status Init(const std::string& name) {
    name_ = name;
    exec_return_type_ = ScalarUDFTraits<T>::ReturnType();
    exec_arguments_ = ScalarUDFTraits<T>::ExecArguments();
    return Status::OK();
  }

  /**
   * Access internal variable exec_return_type.
   * @return the stored return types of the exec function.
   */
  UDFDataType exec_return_type() const { return exec_return_type_; }
  const std::vector<UDFDataType>& exec_arguments() const { return exec_arguments_; }

  /**
   * Access internal variable name.
   * @return Returns the name of the UDF.
   */
  std::string name() { return name_; }

 private:
  std::string name_;
  std::vector<UDFDataType> exec_arguments_;
  UDFDataType exec_return_type_;
};

/**
 * RegistryKey is the class used to uniquely refer to UDFs in the registry.
 * A UDF may be overloaded on exec arguments but nothing else.
 */
class RegistryKey {
 public:
  /**
   * RegistryKey constructor.
   *
   * @param name the name of the UDF.
   * @param exec_args the exec arguments (except FunctionContext).
   */
  RegistryKey(const std::string& name, const std::vector<UDFDataType> exec_args)
      : name_(name), exec_args_(exec_args) {}

  /**
   * Access name of the UDF.
   * @return The name of the udf.
   */
  const std::string& name() const { return name_; }
  /**
   * Access exec aruguments of the UDF.
   * @return a list of exec arguments.
   */
  const std::vector<UDFDataType>& exec_args() const { return exec_args_; }

  /**
   * LessThan operator overload so we can use this in maps.
   * @param lhs is the other RegistryKey.
   * @return a stable less than compare.
   */
  bool operator<(const RegistryKey& lhs) const {
    if (name_ == lhs.name_) {
      return exec_args_ < lhs.exec_args_;
    }
    return name_ < lhs.name_;
  }

 protected:
  std::string name_;
  std::vector<UDFDataType> exec_args_;
};

/**
 * The registry to store UDFs.
 * Right now this only support ScalarUDFs, but its' templated so we can easily extend
 * support to other UDF types.
 *
 * @tparam TUDFDef The UDF defintion to store.
 */
template <typename TUDFDef>
class Registry {
 public:
  explicit Registry(const std::string& name) : name_(name) {}

  /**
   * Registers the given UDF into the registry. A double register will result in an error.
   * @tparam T The UDF to register.
   * @param name The name of the UDF to register.
   * @return Status ok/error.
   */
  template <typename T>
  Status Register(const std::string& name) {
    auto udf_def = std::make_unique<TUDFDef>();
    auto key = RegistryKey(name, ScalarUDFTraits<T>::ExecArguments());
    if (map_.find(key) != map_.end()) {
      return error::AlreadyExists("The UDF with name \"$0\" already exists with same exec args.",
                                  name);
    }
    PL_RETURN_IF_ERROR(udf_def->template Init<T>(name));
    map_[key] = std::move(udf_def);
    return Status::OK();
  }

  /**
   * Get the UDF definition.
   * @param name The name of the UDF.
   * @param exec_args The exec args of the UDF.
   * @return
   */
  StatusOr<TUDFDef*> GetDefinition(const std::string& name,
                                   const std::vector<UDFDataType>& exec_args) {
    auto key = RegistryKey(name, exec_args);
    auto it = map_.find(key);
    if (it == map_.end()) {
      return error::NotFound("No UDF with provided arguments");
    }
    return it->second.get();
  }

  /**
   * Same as Register, except dies when there is an error.
   * @tparam T The UDF to register.
   * @param name The name of the UDF to register.
   */
  template <typename T>
  void RegisterOrDie(const std::string& name) {
    auto status = Register<T>(name);
    CHECK(status.ok()) << "Failed to register UDF: " << status.msg();
  }

  std::string DebugString() {
    std::string debug_string;
    debug_string += absl::StrFormat("Registry: %s\n", name_);
    for (const auto& entry : map_) {
      // TODO(zasgar): add arguments as well. Future Diff.
      debug_string += absl::StrFormat("%s\n", entry.first.name());
    }
    return debug_string;
  }

 private:
  std::string name_;
  std::map<RegistryKey, std::unique_ptr<TUDFDef>> map_;
};

using ScalarUDFRegistry = Registry<ScalarUDFDefinition>;

}  // namespace udf
}  // namespace carnot
}  // namespace pl
