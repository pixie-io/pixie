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

enum RegistryType { kScalarUDF = 1, kUDA };

inline std::string ToString(const RegistryType& registry_type) {
  switch (registry_type) {
    case kScalarUDF:
      return "ScalarUDFRegistry";
    case kUDA:
      return "UDARegistry";
    default:
      return "UnknownRegistry";
  }
}

class UDFDefinition {
 public:
  UDFDefinition() = default;
  virtual ~UDFDefinition() = default;

  /**
   * @return The overload dependent arguments that the registry uses to resolves UDFs.
   */
  virtual const std::vector<UDFDataType>& RegistryArgTypes() = 0;

  /**
   * Access internal variable name.
   * @return Returns the name of the UDF.
   */
  std::string name() { return name_; }

 protected:
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

  const std::vector<UDFDataType>& RegistryArgTypes() override { return exec_arguments_; }

 private:
  std::vector<UDFDataType> exec_arguments_;
  UDFDataType exec_return_type_;
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
    name_ = name;
    update_arguments_ = UDATraits<T>::UpdateArgumentTypes();
    finalize_return_type_ = UDATraits<T>::FinalizeReturnType();
    return Status::OK();
  }

  const std::vector<UDFDataType>& RegistryArgTypes() override { return update_arguments_; }

  const std::vector<UDFDataType> update_arguments() { return update_arguments_; }
  UDFDataType finalize_return_type() const { return finalize_return_type_; }

 private:
  std::vector<UDFDataType> update_arguments_;
  UDFDataType finalize_return_type_;
};

/**
 * RegistryKey is the class used to uniquely refer to UDFs/UDAs in the registry.
 * A UDF may be overloaded on exec arguments but nothing else.
 */
class RegistryKey {
 public:
  /**
   * RegistryKey constructor.
   *
   * @param name the name of the UDF/UDA.
   * @param registry_arg_types the types used for registry resolution (except FunctionContext).
   */
  RegistryKey(const std::string& name, const std::vector<UDFDataType> registry_arg_types)
      : name_(name), registry_arg_types_(registry_arg_types) {}

  /**
   * Access name of the UDF/UDA.
   * @return The name of the udf/uda.
   */
  const std::string& name() const { return name_; }

  const std::vector<UDFDataType> registry_arg_types() { return registry_arg_types_; }

  /**
   * LessThan operator overload so we can use this in maps.
   * @param lhs is the other RegistryKey.
   * @return a stable less than compare.
   */
  bool operator<(const RegistryKey& lhs) const {
    if (name_ == lhs.name_) {
      return registry_arg_types_ < lhs.registry_arg_types_;
    }
    return name_ < lhs.name_;
  }

 protected:
  std::string name_;
  std::vector<UDFDataType> registry_arg_types_;
};

class BaseUDFRegistry {
 public:
  BaseUDFRegistry() = default;
  virtual ~BaseUDFRegistry() = default;
  /**
   * Get the type of the registry.
   * @return Returns the type of the registry.
   */
  virtual RegistryType Type() = 0;
  virtual std::string DebugString() = 0;
};

/**
 * The registry to store UDFs/UDAS.
 *
 * @tparam TUDFDef The UDF defintion to store.
 */
template <typename TUDFDef>
class Registry : public BaseUDFRegistry {
 public:
  explicit Registry(const std::string& name) : name_(name) {}
  ~Registry() override = default;

  /**
   * Registers the given UDF/UDA into the registry. A double register will result in an error.
   * @tparam T The UDF/UDA to register.
   * @param name The name of the UDF/UDA to register.
   * @return Status ok/error.
   */
  template <typename T>
  Status Register(const std::string& name) {
    auto udf_def = std::make_unique<TUDFDef>();
    PL_RETURN_IF_ERROR(udf_def->template Init<T>(name));

    auto key = RegistryKey(name, udf_def->RegistryArgTypes());
    if (map_.find(key) != map_.end()) {
      return error::AlreadyExists("The UDF with name \"$0\" already exists with same exec args.",
                                  name);
    }
    map_[key] = std::move(udf_def);
    return Status::OK();
  }

  /**
   * Same as Register, except dies when there is an error.
   * @tparam T The UDF/UDA to register.
   * @param name The name of the UDF to register.
   */
  template <typename T>
  void RegisterOrDie(const std::string& name) {
    auto status = Register<T>(name);
    CHECK(status.ok()) << "Failed to register UDF: " << status.msg();
  }

  /**
   * Get the UDF/UDA definition.
   * @param name The name of the UDF/UDA.
   * @param registry_arg_types The overload dependent args of the UDF/UDA.
   * @return
   */
  StatusOr<TUDFDef*> GetDefinition(const std::string& name,
                                   const std::vector<UDFDataType>& registry_arg_types) {
    auto key = RegistryKey(name, registry_arg_types);
    auto it = map_.find(key);
    if (it == map_.end()) {
      return error::NotFound("No UDF with provided arguments");
    }
    return it->second.get();
  }

  std::string DebugString() override {
    std::string debug_string;
    debug_string += absl::StrFormat("Registry(%s): %s\n", ToString(Type()), name_);
    for (const auto& entry : map_) {
      // TODO(zasgar): add arguments as well. Future Diff.
      debug_string += absl::StrFormat("%s\n", entry.first.name());
    }
    return debug_string;
  }

 protected:
  std::string name_;
  std::map<RegistryKey, std::unique_ptr<TUDFDef>> map_;
};

class ScalarUDFRegistry : public Registry<ScalarUDFDefinition> {
 public:
  using Registry<ScalarUDFDefinition>::Registry;
  RegistryType Type() override { return kScalarUDF; };
};

class UDARegistry : public Registry<UDADefinition> {
 public:
  using Registry<UDADefinition>::Registry;
  RegistryType Type() override { return kUDA; };
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
