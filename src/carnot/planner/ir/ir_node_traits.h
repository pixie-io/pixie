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
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/types/types.h"

namespace px {
namespace carnot {
namespace planner {

template <typename T, typename = void>
struct ResolveTypeFnHelper : std::false_type {};

template <typename T>
struct ResolveTypeFnHelper<T, std::void_t<decltype(&T::ResolveType)>> : std::true_type {};

template <typename TOpIR>
class OperatorTraits {
  /**
   * @brief Traits object for determining compile time properties of operators.
   *
   * This traits object, currently, allows calling of ResolveType even if the Operator in question
   * doesn't define it. It uses the default resolve type of copying the parent type if an Operator
   * doesn't specify ResolveType. If the Operator specifies FailOnResolveType, then
   * OperatorTraits<T>::ResolveType will result in a CHECK failure.
   *
   * Operators that wish to define a custom ResolveType should define an instance method with
   * signature:
   *  Status ResolveType(CompilerState* compiler_state)
   */
 public:
  static constexpr bool HasResolveType() { return ResolveTypeFnHelper<TOpIR>::value; }

  template <typename Q = TOpIR,
            std::enable_if_t<OperatorTraits<Q>::HasResolveType() && !Q::FailOnResolveType(),
                             std::nullptr_t> = nullptr>
  static auto ResolveType(TOpIR* inst, CompilerState* compiler_state) {
    return inst->ResolveType(compiler_state);
  }

  template <typename Q = TOpIR,
            std::enable_if_t<!OperatorTraits<Q>::HasResolveType() && !Q::FailOnResolveType(),
                             std::nullptr_t> = nullptr>
  static auto ResolveType(TOpIR* inst, CompilerState* /* compiler_state */) {
    PX_ASSIGN_OR_RETURN(auto type_, TOpIR::DefaultResolveType(inst->parent_types()));
    return inst->SetResolvedType(type_);
  }

  template <typename Q = TOpIR,
            std::enable_if_t<!OperatorTraits<Q>::HasResolveType() && Q::FailOnResolveType(),
                             std::nullptr_t> = nullptr>
  static auto ResolveType(TOpIR* inst, CompilerState* /* compiler_state */) {
    // If this function is called, it likely means the ResolveTypes rule in rules.cc is out of
    // order. For instance, it might be being called before group bys are merged into Aggs.
    CHECK(false) << absl::Substitute(
        "Called ResolveType on '$0' which has FailOnResolveType set to true", inst->type_string());
    return Status::OK();
  }
};

// Have to forward declare this for the is_base_of check.
class DataIR;

template <typename TExprIR>
class ExpressionTraits {
  /**
   * @brief Traits object for determining compile time properties of expressions.
   *
   * This traits object, currently, allows calling of ResolveType even if the Expression in question
   * doesn't define it. If the Expression is a DataIR then this traits object defines ResolveType
   * for that expression. If an expression sets FailOnResolveType, then calling
   * ExpressionTraits<T>::ResolveType for that expression will result in a CHECK failure.
   *
   * Expressions that wish to define ResolveType should define an instance method with
   * signature:
   *  Status ResolveType(CompilerState* compiler_state, const std::vector<TypePtr>& parent_types);
   *
   * Note that the parameter `parent_types` represents the types of the containing op's parents.
   */
 public:
  static constexpr bool HasResolveType() { return ResolveTypeFnHelper<TExprIR>::value; }

  template <typename Q = TExprIR,
            std::enable_if_t<ExpressionTraits<Q>::HasResolveType() && !Q::FailOnResolveType(),
                             std::nullptr_t> = nullptr>
  static auto ResolveType(TExprIR* inst, CompilerState* compiler_state,
                          const std::vector<TypePtr>& parent_types) {
    return inst->ResolveType(compiler_state, parent_types);
  }

  template <typename Q = TExprIR,
            std::enable_if_t<!ExpressionTraits<Q>::HasResolveType() && std::is_base_of_v<DataIR, Q>,
                             std::nullptr_t> = nullptr>
  static auto ResolveType(TExprIR* inst, CompilerState* /* compiler_state */,
                          const std::vector<TypePtr>& /* parent_types */) {
    auto type_ = ValueType::Create(inst->EvaluatedDataType(), types::ST_NONE);
    return inst->SetResolvedType(type_);
  }

  template <typename Q = TExprIR,
            std::enable_if_t<Q::FailOnResolveType(), std::nullptr_t> = nullptr>
  static auto ResolveType(TExprIR* inst, CompilerState* /* compiler_state */,
                          const std::vector<TypePtr>& /* parent_types */) {
    // If this function is called, it likely means the ResolveTypes rule in rules.cc is out of
    // order. For instance, it might be being called before MetadataIRs are turned into FuncIRs.
    CHECK(false) << absl::Substitute(
        "Called ResolveType on '$0' which has FailOnResolveType set to true", inst->type_string());
    return Status::OK();
  }
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
