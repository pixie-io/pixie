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
#include <unordered_set>
#include <vector>

#include <absl/container/flat_hash_set.h>
#include "src/carnot/udf/udf.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace udf {

class ExplicitRule;
using ExplicitRulePtr = std::shared_ptr<ExplicitRule>;

class InferenceRule {
 public:
  virtual ~InferenceRule() {}
  /**
   * All inference rules must be converted to explicit rules, since explicit rules map directly to
   * the proto representation of a rule.
   */
  virtual std::vector<ExplicitRulePtr> explicit_rules() = 0;
};
using InfRulePtr = std::shared_ptr<InferenceRule>;
using InfRuleVec = std::vector<InfRulePtr>;

template <typename T, typename = void>
struct has_semantic_inference_rules_fn : std::false_type {};

template <typename T>
struct has_semantic_inference_rules_fn<T, std::void_t<decltype(&T::SemanticInferenceRules)>>
    : std::true_type {};

template <typename T, typename = void>
struct UDFExecTypeHelper {
  static_assert(sizeof(T) == 0, "Unknown UDFExecType");
};

template <typename T>
struct UDFExecTypeHelper<T, typename std::enable_if_t<std::is_base_of_v<UDA, T>, void>> {
  static constexpr auto udf_exec_type = udfspb::UDA;
  static constexpr int num_args = UDATraits<T>::UpdateArgumentTypes().size();
};

template <typename T>
struct UDFExecTypeHelper<T, typename std::enable_if_t<std::is_base_of_v<ScalarUDF, T>, void>> {
  static constexpr auto udf_exec_type = udfspb::SCALAR_UDF;
  static constexpr int num_args = ScalarUDFTraits<T>::ExecArguments().size();
};

template <typename TUDF>
class SemanticInferenceTraits {
  /**
   * @brief Traits of UDFs used to determine the SemanticInferenceRules for that UDF.
   */
 public:
  static constexpr bool HasSemanticInferenceRules() {
    return has_semantic_inference_rules_fn<TUDF>::value;
  }

  static InfRuleVec SemanticInferenceRules() {
    if constexpr (HasSemanticInferenceRules()) {
      return TUDF::SemanticInferenceRules();
    } else {
      return {};
    }
  }

  static constexpr udfspb::UDFExecType udf_exec_type() {
    return UDFExecTypeHelper<TUDF>::udf_exec_type;
  }

  static constexpr int num_args() { return UDFExecTypeHelper<TUDF>::num_args; }
};

class ExplicitRule : public InferenceRule {
  /**
   * @brief Explicitly declared semantic type inference rule.
   *
   * This rule specifies a set of exec (UDF) or update (UDA) arg types and an output type.
   * If a UDF is called with these arg types then this rule specifies the output type.
   *
   * NOTE: Currently, the compiler ignores the init_arg_types.
   */
 public:
  template <typename TUDF>
  static std::shared_ptr<ExplicitRule> Create(
      types::SemanticType out_type, std::vector<types::SemanticType> exec_or_update_types) {
    return std::make_shared<ExplicitRule>(SemanticInferenceTraits<TUDF>::udf_exec_type(), out_type,
                                          exec_or_update_types);
  }
  ExplicitRule(udfspb::UDFExecType udf_exec_type, types::SemanticType out_type,
               std::vector<types::SemanticType> exec_or_update_types)
      : ExplicitRule(udf_exec_type, out_type, {}, exec_or_update_types) {}
  ExplicitRule(udfspb::UDFExecType udf_exec_type, types::SemanticType out_type,
               std::vector<types::SemanticType> init_arg_types,
               std::vector<types::SemanticType> exec_or_update_types)
      : udf_exec_type_(udf_exec_type),
        out_type_(out_type),
        init_arg_types_(init_arg_types),
        exec_or_update_types_(exec_or_update_types) {}

  void ToProto(const std::string& name, udfspb::SemanticInferenceRule* rule) const;
  size_t Hash() const;
  bool Equals(const ExplicitRulePtr& other) const;
  std::vector<ExplicitRulePtr> explicit_rules() {
    return {std::make_shared<ExplicitRule>(udf_exec_type_, out_type_, init_arg_types_,
                                           exec_or_update_types_)};
  }

 private:
  udfspb::UDFExecType udf_exec_type_;
  types::SemanticType out_type_;
  std::vector<types::SemanticType> init_arg_types_;
  std::vector<types::SemanticType> exec_or_update_types_;
};

struct explicit_rules_eq {
  bool operator()(const ExplicitRulePtr& lhs, const ExplicitRulePtr& rhs) const {
    return lhs->Equals(rhs);
  }
};

struct explicit_rules_hash {
  size_t operator()(const ExplicitRulePtr& val) const { return val->Hash(); }
};

using ExplicitRuleSet = std::unordered_set<ExplicitRulePtr, explicit_rules_hash, explicit_rules_eq>;

template <typename TUDF>
class InheritTypeFromArgs : public InferenceRule {
  /**
   * @brief This rule specifies that a UDF should inherit certain semantic types from the types of
   * the args passed in.
   *
   * NOTE: this rule generates multiple ExplicitRules.
   */
 public:
  static std::shared_ptr<InheritTypeFromArgs<TUDF>> Create(
      std::vector<types::SemanticType> types_to_match) {
    return Create(types_to_match, {});
  }

  /**
   * @param types_to_match specifies which set of types the UDF should be allowed to inherit from
   * the arg types.
   * @param arg_indices specifies which indices of the arguments have to match the type, in order
   * for the output to be of that type. If the arg indices are empty then all the input args must
   * have that type.
   *
   * As an example, if you create this rule for an AddUDF that takes in 2 arguments, like so:
   * `InheritTypeFromArgs<AddUDF>::Create({ST_BYTES, ST_PERCENT, ST_DURATION}, {});`
   * or equivalently:
   * `InheritTypeFromArgs<AddUDF>::Create({ST_BYTES, ST_PERCENT, ST_DURATION}, {0, 1});`
   * then this is saying the following:
   * ST_BYTES + ST_BYTES -> ST_BYTES
   * ST_PERCENT + ST_PERCENT -> ST_PERCENT
   * ST_DURATION + ST_DURATION -> ST_DURATION
   *
   * As another example, suppose you have a CustomUDF that takes 2 arguments, and create the
   * following rule:
   * ```
   * rule = InheritTypeFromArgs<CustomUDF>::Create({ST_SERVICE_NAME, ST_POD_NAME}, {1});
   * ```
   *  then this is saying that, if you pass a ST_SERVICE_NAME or ST_POD_NAME as the second argument
   * of this udf then it produces that same type in the output. For instance,
   * CustomUDF::Exec(ST_NONE, ST_SERVICE_NAME) will have semantic type ST_SERVICE_NAME, even
   * though the first arg is unspecified, since in creating the rule, the arg_indices were set to
   * {1}, meaning only the 2nd arg matters.
   */
  static std::shared_ptr<InheritTypeFromArgs<TUDF>> Create(
      std::vector<types::SemanticType> types_to_match, absl::flat_hash_set<int> arg_indices) {
    return std::make_shared<InheritTypeFromArgs<TUDF>>(types_to_match, arg_indices);
  }

  static std::shared_ptr<InheritTypeFromArgs<TUDF>> CreateGeneric() {
    return CreateGeneric(absl::flat_hash_set<int>({}));
  }

  /**
   * @param arg_indices specifies which indices of the arguments have to match the type, in order
   * for the output to be of that type. If the arg indices are empty then all the input args must
   * have that type.
   *
   * Creates to inherit output semantic type from the input columns. While `Create` does a similar
   * thing, it requires that specific semantic types be specified. `CreateGeneric` will work for any
   * semantic type, including new ones that are added in the future. It should be used in the subset
   * of cases where the semantic type should always be propagated if the args match.
   */
  static std::shared_ptr<InheritTypeFromArgs<TUDF>> CreateGeneric(
      absl::flat_hash_set<int> arg_indices) {
    return std::make_shared<InheritTypeFromArgs<TUDF>>(GetAllSemanticTypes(), arg_indices);
  }

  explicit InheritTypeFromArgs(std::vector<types::SemanticType> types_to_match)
      : InheritTypeFromArgs(types_to_match, {}) {}
  InheritTypeFromArgs(std::vector<types::SemanticType> types_to_match,
                      absl::flat_hash_set<int> arg_indices) {
    auto num_args = SemanticInferenceTraits<TUDF>::num_args();
    for (const auto& type : types_to_match) {
      std::vector<types::SemanticType> arg_types;
      for (int i = 0; i < num_args; i++) {
        if (arg_indices.size() == 0 || arg_indices.contains(i)) {
          arg_types.push_back(type);
        } else {
          arg_types.push_back(types::ST_NONE);
        }
      }
      rules_.push_back(ExplicitRule::Create<TUDF>(type, arg_types));
    }
  }

  std::vector<ExplicitRulePtr> explicit_rules() { return rules_; }

 private:
  static std::vector<types::SemanticType> GetAllSemanticTypes() {
    std::vector<types::SemanticType> types;
    for (int i = types::SemanticType_MIN; i <= types::SemanticType_MAX; ++i) {
      if (types::SemanticType_IsValid(i)) {
        types.push_back(static_cast<types::SemanticType>(i));
      }
    }
    return types;
  }

  std::vector<ExplicitRulePtr> rules_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
