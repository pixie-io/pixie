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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>

#include "src/carnot/udf/doc.h"
#include "src/carnot/udf/type_inference.h"
#include "src/carnot/udf/udf_definition.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace udf {

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

  std::string DebugString() const;

  const std::vector<types::DataType> registry_arg_types() { return registry_arg_types_; }

  /**
   * LessThan operator overload so we can use this in maps.
   * @param lhs is the other RegistryKey.
   * @return a stable less than compare.
   */
  bool operator<(const RegistryKey& lhs) const;

 protected:
  std::string name_;
  std::vector<types::DataType> registry_arg_types_;
};

template <typename T, typename = void>
struct RegistryTraits {
  static_assert(sizeof(T) == 0, "Invalid UDF type");
};

template <typename T>
struct RegistryTraits<T, typename std::enable_if_t<std::is_base_of_v<ScalarUDF, T>, void>> {
  using TUDFDef = ScalarUDFDefinition;
};

template <typename T>
struct RegistryTraits<T, typename std::enable_if_t<std::is_base_of_v<UDA, T>, void>> {
  using TUDFDef = UDADefinition;
};

template <typename T>
struct RegistryTraits<T, typename std::enable_if_t<std::is_base_of_v<UDTF<T>, T>, void>> {
  using TUDFDef = UDTFDefinition;
};

/**
 * The registry to store UDFs/UDAS/UDTFs.
 *
 */
class Registry {
 public:
  using RegistryMap = std::map<RegistryKey, std::unique_ptr<UDFDefinition>>;
  explicit Registry(std::string name) : name_(std::move(name)) {}
  virtual ~Registry() = default;

  /**
   * Registers the given UDF/UDA/UDTF into the registry. A double register will result in an error.
   * @tparam T The UDF/UDA/UDTF to register.
   * @param name The name of the UDF/UDA/UDTF to register.
   * @return Status ok/error.
   */
  template <typename T>
  Status Register(const std::string& name) {
    auto udf_def = std::make_unique<typename RegistryTraits<T>::TUDFDef>(name);
    PX_RETURN_IF_ERROR(udf_def->template Init<T>());

    auto key = RegistryKey(name, udf_def->RegistryArgTypes());
    if (map_.find(key) != map_.end()) {
      return error::AlreadyExists(
          "The UDF with name \"$0\" already exists with the same arg types \"$1\".", name,
          key.DebugString());
    }
    map_[key] = std::move(udf_def);
    RegisterSemanticTypes<T>(name);

    if constexpr (has_valid_doc_fn<T>()) {
      auto docpb = docs_pb_.add_udf();
      PX_RETURN_IF_ERROR(T::Doc().template ToProto<T>(docpb));
      docpb->set_name(name);
    }
    return Status::OK();
  }

  /**
   * Registers all semantic inference rules declared in T::SemanticInferenceRules or none if the
   * function isn't defined on T.
   *
   * @param name name to register the rules under.
   */
  template <typename T>
  void RegisterSemanticTypes(const std::string& name) {
    auto rules = SemanticInferenceTraits<T>::SemanticInferenceRules();
    auto it = semantic_type_rules_.find(name);
    if (it == semantic_type_rules_.end()) {
      semantic_type_rules_[name] = ExplicitRuleSet();
    }
    for (auto& rule : rules) {
      for (auto& explicit_rule : rule->explicit_rules()) {
        if (semantic_type_rules_[name].find(explicit_rule) != semantic_type_rules_[name].end()) {
          // We have to deduplicate the rules since there could be overloaded primitive data types,
          // causing multiple of the same semantic type rules to be registered.
          continue;
        }
        semantic_type_rules_[name].insert(explicit_rule);
      }
    }
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
    static_assert(std::is_base_of_v<UDTF<TUDTF>, TUDTF>,
                  "Register factory is only valid for UDTFs");

    auto factory = std::make_unique<TFactory>(std::forward<Args>(args)...);
    auto udf_def = std::make_unique<UDTFDefinition>(name);
    PX_RETURN_IF_ERROR(udf_def->template Init<TUDTF>(std::move(factory)));

    auto key = RegistryKey(name, udf_def->RegistryArgTypes());
    if (map_.find(key) != map_.end()) {
      return error::AlreadyExists(
          "The UDTF with name \"$0\" already exists with same exec args \"$1\".", name,
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

  StatusOr<ScalarUDFDefinition*> GetScalarUDFDefinition(
      const std::string& name, const std::vector<types::DataType>& registry_arg_types = {}) const;

  StatusOr<UDADefinition*> GetUDADefinition(
      const std::string& name, const std::vector<types::DataType>& registry_arg_types = {}) const;

  StatusOr<UDTFDefinition*> GetUDTFDefinition(
      const std::string& name, const std::vector<types::DataType>& registry_arg_types = {}) const;

  std::string DebugString() const;
  udfspb::UDFInfo ToProto();

  const RegistryMap& map() const { return map_; }

  /**
   * Returns the UDF docs proto.
   */
  const udfspb::Docs& ToDocsProto() { return docs_pb_; }

 private:
  /**
   * Get the UDF/UDA/UDTF definition.
   * @param name The name of the UDF/UDA/UDTF.
   * @return
   */
  StatusOr<UDFDefinition*> GetDefinition(
      const std::string& name, const std::vector<types::DataType>& registry_arg_types = {}) const;

  void ToProto(const ScalarUDFDefinition& def, udfspb::ScalarUDFSpec* spec);
  void ToProto(const UDADefinition& def, udfspb::UDASpec* spec);
  void ToProto(const UDTFDefinition& def, udfspb::UDTFSourceSpec* spec);

  std::string name_;
  RegistryMap map_;
  std::map<std::string, ExplicitRuleSet> semantic_type_rules_;
  udfspb::Docs docs_pb_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
