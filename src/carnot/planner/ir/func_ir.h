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

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/data_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief Represents functions with arbitrary number of values
 */
class FuncIR : public ExpressionIR {
 public:
  enum Opcode {
    non_op = -1,
    mult,
    sub,
    add,
    div,
    eq,
    neq,
    lteq,
    gteq,
    lt,
    gt,
    logand,
    logor,
    mod,
    negate,
    lognot,
    invert,
    number_of_ops
  };
  struct Op {
    Opcode op_code;
    std::string python_op;
    std::string carnot_op_name;
  };
  static std::unordered_map<std::string, Op> op_map;
  static std::unordered_map<std::string, Op> unary_op_map;

  FuncIR() = delete;
  Opcode opcode() const { return op_.op_code; }
  const Op& op() const { return op_; }
  FuncIR(int64_t id, const ExpressionIR::Annotations& annotations)
      : ExpressionIR(id, IRNodeType::kFunc, annotations) {}
  explicit FuncIR(int64_t id) : FuncIR(id, ExpressionIR::Annotations()) {}
  Status Init(Op op, const std::vector<ExpressionIR*>& args);

  std::string DebugString() const override {
    return absl::Substitute("$0(id=$1, $2)", func_name(), id(),
                            absl::StrJoin(all_args_, ",", [](std::string* out, IRNode* in) {
                              absl::StrAppend(out, in->DebugString());
                            }));
  }

  std::string func_name() const { return op_.carnot_op_name; }
  std::string carnot_op_name() const { return op_.carnot_op_name; }

  int64_t func_id() const { return func_id_; }
  // all_args() is both init arguments and exec/update arguments.
  const std::vector<ExpressionIR*>& all_args() const { return all_args_; }
  // args() is only exec/update arguments and can't be called before SplitInitArgs is called.
  const std::vector<ExpressionIR*>& args() const {
    DCHECK(is_init_args_split_) << "Must call SplitInitArgs before args()";
    return args_;
  }
  const std::vector<types::DataType>& registry_arg_types() const { return registry_arg_types_; }
  bool HasRegistryArgTypes() const {
    if (!is_init_args_split_) {
      return registry_arg_types_.size() == all_args_.size();
    }
    return registry_arg_types_.size() == (init_args_.size() + args_.size());
  }
  // UpdateArg updates an arg relative to an index into all_args.
  Status UpdateArg(int64_t idx, ExpressionIR* arg);
  Status UpdateArg(ExpressionIR* old_arg, ExpressionIR* new_arg);

  Status AddArg(ExpressionIR* arg);

  types::DataType EvaluatedDataType() const override {
    if (!is_type_resolved()) {
      return types::DATA_TYPE_UNKNOWN;
    }
    return resolved_value_type()->data_type();
  }
  bool IsDataTypeEvaluated() const override { return is_type_resolved(); }
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  Status ToProto(planpb::ScalarExpression* expr) const override;
  bool IsFunction() const override { return true; }
  static bool NodeMatches(IRNode* input);
  static std::string class_type_string() { return "Func"; }

  bool Equals(ExpressionIR* expr) const override {
    if (!NodeMatches(expr)) {
      return false;
    }
    auto func = static_cast<FuncIR*>(expr);
    if (func->func_name() != func_name() || func->IsInitArgsSplit() != IsInitArgsSplit() ||
        func->all_args_.size() != all_args_.size() ||
        func->init_args_.size() != init_args_.size() || args_.size() != args_.size()) {
      return false;
    }
    for (const auto& [idx, node] : Enumerate(func->all_args_)) {
      if (!all_args_[idx]->Equals(node)) {
        return false;
      }
    }
    for (const auto& [idx, node] : Enumerate(func->init_args_)) {
      if (!init_args_[idx]->Equals(node)) {
        return false;
      }
    }
    for (const auto& [idx, node] : Enumerate(func->args_)) {
      if (!args_[idx]->Equals(node)) {
        return false;
      }
    }
    return true;
  }

  Status ResolveType(CompilerState* compiler_state, const std::vector<TypePtr>& parent_types);

  bool SupportsPartial() const { return supports_partial_; }

  const std::vector<DataIR*>& init_args() const {
    DCHECK(is_init_args_split_) << "Must call SplitInitArgs before init_args()";
    return init_args_;
  }
  bool IsInitArgsSplit() const { return is_init_args_split_; }

 private:
  std::string func_prefix_ = kPLFuncPrefix;
  Op op_;
  std::string func_name_;
  std::vector<ExpressionIR*> all_args_;
  std::vector<DataIR*> init_args_;
  std::vector<ExpressionIR*> args_;
  std::vector<types::DataType> registry_arg_types_;
  int64_t func_id_ = 0;
  bool supports_partial_ = false;
  bool is_init_args_split_ = false;

  // Adds the arg if it isn't already present in the func, otherwise clones it so that there is no
  // duplicate edge.
  Status AddOrCloneArg(ExpressionIR* arg);
  Status AddInitArg(DataIR* arg);
  Status SetInfoFromRegistry(CompilerState* compiler_state,
                             const std::vector<types::DataType>& registry_arg_types);
  Status SplitInitArgs(size_t num_init_args);
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
