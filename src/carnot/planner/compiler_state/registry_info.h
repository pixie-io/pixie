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

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_format.h>
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

namespace px {
namespace carnot {
namespace planner {

enum class UDFExecType { kUDA = 0, kUDF = 1 };

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
  RegistryKey(std::string name, const std::vector<types::DataType> registry_arg_types)
      : name_(std::move(name)), registry_arg_types_(registry_arg_types) {}

  /**
   * Access name of the UDF/UDA.
   * @return The name of the udf/uda.
   */
  const std::string& name() const { return name_; }

  const std::vector<types::DataType>& registry_arg_types() { return registry_arg_types_; }

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

class SemanticRuleRegistry {
  /**
   * SemanticRuleRegistry stores a set of semantic inference rules and provides functionality to
   * lookup a rule based on the input types to a udf/uda.
   *
   * If there are ST_NONE semantic types in the rule then they are treated as a catch all
   * (i.e. they will match any semantic type).
   *
   * If there are multiple matching rules, it will pick the most specific rule, i.e. the rule with
   * the least number of ST_NONE types that matched other types as above.
   */
  using ArgTypes = std::vector<types::SemanticType>;
  using TypeSet = std::vector<std::pair<ArgTypes, types::SemanticType>>;

 public:
  /**
   * @brief insert a semantic inference rule into the registry.
   *
   * @param name name of udf/uda the rule applies to.
   * @param arg_types argument semantic types to match to.
   * @param out_type output semantic type of the udf/uda if the argument types are matched.
   */
  void Insert(std::string name, const ArgTypes& arg_types, types::SemanticType out_type);

  /**
   * @brief lookup any matching semantic inference rules.
   *
   * @param name name of udf/uda to lookup rules for.
   * @param arg_types semantic types of the arguments of the func being called.
   * @return if the arg_types match a rule for this function, then it returns the output semantic
   * type of that rule, otherwise it returns an error.
   */
  StatusOr<types::SemanticType> Lookup(std::string name, const ArgTypes& arg_types) const;

 protected:
  std::pair<bool, int> MatchesCandidateAtDist(const ArgTypes& arg_types,
                                              const ArgTypes& arg_types_candidate) const;

 private:
  std::map<std::string, TypeSet> map_;
};

class RegistryInfo {
 public:
  Status Init(const udfspb::UDFInfo& info);
  StatusOr<types::DataType> GetUDADataType(std::string name,
                                           std::vector<types::DataType> arg_types);
  StatusOr<types::DataType> GetUDFDataType(std::string name,
                                           std::vector<types::DataType> arg_types);
  StatusOr<udfspb::UDFSourceExecutor> GetUDFSourceExecutor(std::string name,
                                                           std::vector<types::DataType> arg_types);

  StatusOr<bool> DoesUDASupportPartial(std::string name, std::vector<types::DataType> arg_types);

  StatusOr<UDFExecType> GetUDFExecType(std::string_view name);
  absl::flat_hash_set<std::string> func_names() const;

  std::vector<udfspb::UDTFSourceSpec> udtfs() const { return udtfs_; }

  // TODO(philkuz) move this function to protected when udtfs are finally supported.
  void AddUDTF(const udfspb::UDTFSourceSpec& source_spec) { udtfs_.push_back(source_spec); }

  const udfspb::UDFInfo& info_pb() { return info_pb_; }

  /**
   * @brief Resolves the semantic and data types of the UDF/UDA.
   *
   * If no semantic inference rule is found in the SemanticRuleRegistry, then
   * ST_NONE is returned.
   * @param name name of udf/uda to resolve type for.
   * @param arg_types list of ValueTypes for each arg to the udf/uda.
   * @return A ValueType representing the type of the output of the udf/uda.
   */
  StatusOr<std::shared_ptr<ValueType>> ResolveUDFType(
      std::string name, const std::vector<std::shared_ptr<ValueType>>& arg_types);

  template <typename TType>
  StatusOr<TType> ResolveUDFSubType(std::string name, std::vector<TType> arg_types);

  StatusOr<size_t> GetNumInitArgs(std::string name, const std::vector<types::DataType>& arg_types);

 protected:
  void AddSemanticInferenceRule(const udfspb::SemanticInferenceRule& rule);

  std::map<RegistryKey, types::DataType> udf_map_;
  std::map<RegistryKey, types::DataType> uda_map_;
  std::map<RegistryKey, udfspb::UDFSourceExecutor> udf_executor_map_;

  std::map<RegistryKey, size_t> num_init_args_map_;

  // Allocated as a separate map because this is a temporary solution.
  std::map<RegistryKey, bool> uda_supports_partial_map_;
  // Union of udf and uda names.
  absl::flat_hash_map<std::string, UDFExecType> funcs_;
  // The vector containing udtfs.
  std::vector<udfspb::UDTFSourceSpec> udtfs_;

  udfspb::UDFInfo info_pb_;

  SemanticRuleRegistry semantic_rule_registry_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
