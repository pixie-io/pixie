#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include <magic_enum.hpp>

#include "src/carnot/udf/udf_definition.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace udf {

enum RegistryType { kScalarUDF = 1, kUDA, kUDTF };

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
  RegistryKey(std::string name, std::vector<types::DataType> registry_arg_types)
      : name_(std::move(name)), registry_arg_types_(std::move(registry_arg_types)) {}

  /**
   * Access name of the UDF/UDA.
   * @return The name of the udf/uda.
   */
  const std::string& name() const { return name_; }

  std::string DebugString() const {
    std::vector<std::string> name_vector;
    for (auto const& type : registry_arg_types_) {
      name_vector.push_back(types::DataType_Name(type));
    }
    return absl::Substitute("$0($1)", name_, absl::StrJoin(name_vector, ","));
  }

  const std::vector<types::DataType> registry_arg_types() { return registry_arg_types_; }

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
  std::vector<types::DataType> registry_arg_types_;
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
  virtual udfspb::UDFInfo SpecToProto() const = 0;
};

/**
 * The registry to store UDFs/UDAS.
 *
 * @tparam TUDFDef The UDF defintion to store.
 */
template <typename TUDFDef>
class Registry : public BaseUDFRegistry {
 public:
  explicit Registry(std::string name) : name_(std::move(name)) {}
  ~Registry() override = default;

  /**
   * Registers the given UDF/UDA/UDTF into the registry. A double register will result in an error.
   * @tparam T The UDF/UDA/UDTF to register.
   * @param name The name of the UDF/UDA/UDTF to register.
   * @return Status ok/error.
   */
  template <typename T>
  Status Register(const std::string& name) {
    auto udf_def = std::make_unique<TUDFDef>();
    PL_RETURN_IF_ERROR(udf_def->template Init<T>(name));

    auto key = RegistryKey(name, udf_def->RegistryArgTypes());
    if (map_.find(key) != map_.end()) {
      return error::AlreadyExists(
          "The UDF with name \"$0\" already exists with same exec args \"$1\".", name,
          key.DebugString());
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
  StatusOr<TUDFDef*> GetDefinition(
      const std::string& name, const std::vector<types::DataType>& registry_arg_types = {}) const {
    auto key = RegistryKey(name, registry_arg_types);
    auto it = map_.find(key);
    if (it == map_.end()) {
      return error::NotFound("No UDF matching $0 found.", key.DebugString());
    }
    return it->second.get();
  }

  std::string DebugString() override {
    std::string debug_string;
    debug_string += absl::Substitute("Registry($0): $1\n", magic_enum::enum_name(Type()), name_);
    for (const auto& entry : map_) {
      // TODO(zasgar): add arguments as well. Future Diff.
      debug_string += entry.first.name() + "\n";
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
  udfspb::UDFInfo SpecToProto() const override;
};

class UDARegistry : public Registry<UDADefinition> {
 public:
  using Registry<UDADefinition>::Registry;
  RegistryType Type() override { return kUDA; };
  udfspb::UDFInfo SpecToProto() const override;
};

class UDTFRegistry : public Registry<UDTFDefinition> {
 public:
  using Registry<UDTFDefinition>::Registry;
  RegistryType Type() override { return kUDTF; };
  udfspb::UDFInfo SpecToProto() const override;

  /**
   * UDTF specfic Register function that creates a UDTF factory.
   * @tparam TUDTF the UDTF
   * @tparam TFactory the UDTF factory
   * @param name name of the udtf
   * @return
   */
  template <typename TUDTF, typename TFactory, typename... Args>
  Status RegisterFactory(const std::string& name, Args&&... args) {
    static_assert(std::is_base_of_v<UDTFFactory, TFactory>,
                  "TFactory must be derived from UDTFFactory");

    auto factory = std::make_unique<TFactory>(std::forward<Args>(args)...);
    auto udf_def = std::make_unique<UDTFDefinition>();
    PL_RETURN_IF_ERROR(udf_def->template Init<TUDTF>(std::move(factory), name));

    auto key = RegistryKey(name, udf_def->RegistryArgTypes());
    if (map_.find(key) != map_.end()) {
      return error::AlreadyExists(
          "The UDF with name \"$0\" already exists with same exec args \"$1\".", name,
          key.DebugString());
    }
    map_[key] = std::move(udf_def);
    return Status::OK();
  }

  /**
   * Same as Register, except dies when there is an error.
   */
  template <typename TUDTF, typename TFactory, typename... Args>
  void RegisterFactoryOrDie(const std::string& name, Args&&... args) {
    auto status = RegisterFactory<TUDTF, TFactory>(name, std::forward<Args>(args)...);
    CHECK(status.ok()) << "Failed to register UDTF: " << status.msg();
  }
};

/**
 * RegistryInfoExporter is a helper class used to export info from various
 * registries. Usage:
 *   RegistryInfoExporter()
 *     .Registry(registry1)
 *     .Registry(registry2)
 *     .ToProto()
 */
class RegistryInfoExporter {
 public:
  RegistryInfoExporter& Registry(const BaseUDFRegistry& registry) {
    auto info = registry.SpecToProto();
    info_.MergeFrom(info);
    return *this;
  }
  udfspb::UDFInfo ToProto() { return info_; }

 private:
  udfspb::UDFInfo info_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
