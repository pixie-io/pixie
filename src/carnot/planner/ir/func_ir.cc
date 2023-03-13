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

#include "src/carnot/planner/ir/func_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {

std::unordered_map<std::string, FuncIR::Op> FuncIR::op_map{
    {"*", {FuncIR::Opcode::mult, "*", "multiply"}},
    {"+", {FuncIR::Opcode::add, "+", "add"}},
    {"%", {FuncIR::Opcode::mod, "%", "modulo"}},
    {"-", {FuncIR::Opcode::sub, "-", "subtract"}},
    {"/", {FuncIR::Opcode::div, "/", "divide"}},
    {">", {FuncIR::Opcode::gt, ">", "greaterThan"}},
    {"<", {FuncIR::Opcode::lt, "<", "lessThan"}},
    {"==", {FuncIR::Opcode::eq, "==", "equal"}},
    {"!=", {FuncIR::Opcode::neq, "!=", "notEqual"}},
    {"<=", {FuncIR::Opcode::lteq, "<=", "lessThanEqual"}},
    {">=", {FuncIR::Opcode::gteq, ">=", "greaterThanEqual"}},
    {"and", {FuncIR::Opcode::logand, "and", "logicalAnd"}},
    {"or", {FuncIR::Opcode::logor, "or", "logicalOr"}}};

std::unordered_map<std::string, FuncIR::Op> FuncIR::unary_op_map{
    // + as a unary operator returns its operand unchanged.
    {"+", {FuncIR::Opcode::non_op, "+", ""}},
    {"-", {FuncIR::Opcode::negate, "-", "negate"}},
    {"not", {FuncIR::Opcode::lognot, "not", "logicalNot"}},
    {"~", {FuncIR::Opcode::invert, "~", "invert"}},
};

Status FuncIR::Init(Op op, const std::vector<ExpressionIR*>& args) {
  op_ = op;
  for (auto a : args) {
    PX_RETURN_IF_ERROR(AddOrCloneArg(a));
  }
  return Status::OK();
}

Status FuncIR::AddArg(ExpressionIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Argument for FuncIR is null.");
  }
  all_args_.push_back(arg);
  if (is_init_args_split_) {
    args_.push_back(arg);
  }
  return graph()->AddEdge(this, arg);
}

Status FuncIR::AddInitArg(DataIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Init argument for FuncIR is null.");
  }
  init_args_.push_back(arg);
  return graph()->AddEdge(this, arg);
}

Status FuncIR::UpdateArg(int64_t idx, ExpressionIR* arg) {
  CHECK_LT(idx, static_cast<int64_t>(all_args_.size()))
      << "Tried to update arg of index greater than number of args.";
  ExpressionIR* old_arg = all_args_[idx];
  all_args_[idx] = arg;
  if (is_init_args_split_) {
    if (idx < static_cast<int64_t>(init_args_.size())) {
      if (!Match(arg, DataNode())) {
        return CreateIRNodeError(
            "expected init argument $0 to be a primitive data type, received $1", idx,
            arg->type_string());
      }
      init_args_[idx] = static_cast<DataIR*>(arg);
    } else {
      args_[idx - init_args_.size()] = arg;
    }
  }
  PX_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_arg));
  PX_RETURN_IF_ERROR(graph()->OptionallyCloneWithEdge(this, arg));
  PX_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_arg->id()));
  return Status::OK();
}

Status FuncIR::UpdateArg(ExpressionIR* old_arg, ExpressionIR* new_arg) {
  for (size_t i = 0; i < all_args_.size(); ++i) {
    if (all_args_[i] != old_arg) {
      continue;
    }
    return UpdateArg(i, new_arg);
  }
  return error::Internal("Arg $0 does not exist in $1", old_arg->DebugString(), DebugString());
}

Status FuncIR::AddOrCloneArg(ExpressionIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Argument for FuncIR is null.");
  }
  PX_ASSIGN_OR_RETURN(auto updated_arg, graph()->OptionallyCloneWithEdge(this, arg));
  all_args_.push_back(updated_arg);
  return Status::OK();
}

Status FuncIR::ToProto(planpb::ScalarExpression* expr) const {
  auto func_pb = expr->mutable_func();
  func_pb->set_name(func_name());
  func_pb->set_id(func_id_);
  for (DataIR* init_arg : init_args()) {
    PX_RETURN_IF_ERROR(init_arg->ToProto(func_pb->add_init_args()));
  }
  if (!HasRegistryArgTypes()) {
    return CreateIRNodeError("arg types not resolved");
  }
  for (const auto& [idx, arg] : Enumerate(args())) {
    PX_RETURN_IF_ERROR(arg->ToProto(func_pb->add_args()));
    func_pb->add_args_data_types(registry_arg_types_[idx + init_args().size()]);
  }
  return Status::OK();
}

Status FuncIR::SplitInitArgs(size_t num_init_args) {
  if (all_args_.size() < num_init_args) {
    return CreateIRNodeError("expected $0 init args but only received $1 arguments", num_init_args,
                             all_args_.size());
  }
  for (const auto& [idx, arg] : Enumerate(all_args_)) {
    if (idx < num_init_args) {
      if (!Match(arg, DataNode())) {
        return CreateIRNodeError(
            "expected init argument $0 to be a primitive data type, received $1", idx,
            arg->type_string());
      }
      init_args_.push_back(static_cast<DataIR*>(arg));
    } else {
      args_.push_back(arg);
    }
  }
  is_init_args_split_ = true;
  return Status::OK();
}

Status FuncIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const FuncIR* func = static_cast<const FuncIR*>(node);
  func_prefix_ = func->func_prefix_;
  op_ = func->op_;
  func_name_ = func->func_name_;
  registry_arg_types_ = func->registry_arg_types_;
  func_id_ = func->func_id_;
  supports_partial_ = func->supports_partial_;
  is_init_args_split_ = func->is_init_args_split_;

  for (const DataIR* init_arg : func->init_args_) {
    PX_ASSIGN_OR_RETURN(DataIR * new_arg, graph()->CopyNode(init_arg, copied_nodes_map));
    PX_RETURN_IF_ERROR(AddInitArg(new_arg));
  }

  for (const ExpressionIR* arg : func->args_) {
    PX_ASSIGN_OR_RETURN(ExpressionIR * new_arg, graph()->CopyNode(arg, copied_nodes_map));
    PX_RETURN_IF_ERROR(AddArg(new_arg));
  }

  // This branch is hit only when the above two for loops looped over nothing.
  if (!is_init_args_split_) {
    for (const ExpressionIR* arg : func->all_args_) {
      PX_ASSIGN_OR_RETURN(ExpressionIR * new_arg, graph()->CopyNode(arg, copied_nodes_map));
      all_args_.push_back(new_arg);
      PX_RETURN_IF_ERROR(graph()->AddEdge(this, new_arg));
    }
  }
  return Status::OK();
}

bool FuncIR::NodeMatches(IRNode* node) { return Match(node, Func()); }

Status FuncIR::SetInfoFromRegistry(CompilerState* compiler_state,
                                   const std::vector<types::DataType>& registry_arg_types) {
  registry_arg_types_ = registry_arg_types;

  auto udftype_or_s = compiler_state->registry_info()->GetUDFExecType(func_name());
  if (!udftype_or_s.ok()) {
    return CreateIRNodeError(udftype_or_s.status().msg());
  }

  if (!is_init_args_split_) {
    PX_ASSIGN_OR_RETURN(size_t num_init_args, compiler_state->registry_info()->GetNumInitArgs(
                                                  func_name(), registry_arg_types));
    PX_RETURN_IF_ERROR(SplitInitArgs(num_init_args));
  }

  std::vector<uint64_t> init_arg_hashes;
  for (const auto& init_arg : init_args()) {
    init_arg_hashes.push_back(init_arg->HashValue());
  }
  switch (udftype_or_s.ConsumeValueOrDie()) {
    case UDFExecType::kUDF: {
      func_id_ =
          compiler_state->GetUDFID(IDRegistryKey(func_name(), registry_arg_types, init_arg_hashes));
      break;
    }
    case UDFExecType::kUDA: {
      PX_ASSIGN_OR_RETURN(supports_partial_, compiler_state->registry_info()->DoesUDASupportPartial(
                                                 func_name(), registry_arg_types));
      func_id_ =
          compiler_state->GetUDAID(IDRegistryKey(func_name(), registry_arg_types, init_arg_hashes));
      break;
    }
    default: {
      return error::Internal("Unsupported UDFExecType");
    }
  }
  return Status::OK();
}

Status FuncIR::ResolveType(CompilerState* compiler_state,
                           const std::vector<TypePtr>& parent_types) {
  // Resolve the arg types first.
  std::vector<ValueTypePtr> arg_types;
  std::vector<types::DataType> arg_data_types;
  for (auto expr : all_args_) {
    PX_RETURN_IF_ERROR(ResolveExpressionType(expr, compiler_state, parent_types));
    auto type = expr->resolved_value_type();
    arg_types.push_back(type);
    arg_data_types.push_back(type->data_type());
  }

  // In the future, we should refactor RegistryInfo to work with ValueTypePtrs instead of
  // types::DataTypes.
  PX_RETURN_IF_ERROR(SetInfoFromRegistry(compiler_state, arg_data_types));

  PX_ASSIGN_OR_RETURN(auto type_,
                      compiler_state->registry_info()->ResolveUDFType(func_name(), arg_types));
  return SetResolvedType(type_);
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
