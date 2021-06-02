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

#include <string>
#include <vector>

#include "src/carnot/planner/compiler/analyzer/data_type_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<bool> DataTypeRule::Apply(IRNode* ir_node) {
  if (Match(ir_node, UnresolvedRTFuncMatchAllArgs(ResolvedExpression()))) {
    // Match any function that has all args resolved.
    return EvaluateFunc(compiler_state_, static_cast<FuncIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedColumnType())) {
    // Evaluate any unresolved columns.
    return EvaluateColumn(static_cast<ColumnIR*>(ir_node));
  } else if (Match(ir_node, UnresolvedMetadataType())) {
    // Evaluate any unresolved columns.
    return EvaluateMetadata(static_cast<MetadataIR*>(ir_node));
  }
  return false;
}

StatusOr<bool> DataTypeRule::EvaluateFunc(CompilerState* compiler_state, FuncIR* func) {
  // Get the types of the children of this function.
  std::vector<types::DataType> children_data_types;
  for (const auto& arg : func->args()) {
    types::DataType t = arg->EvaluatedDataType();
    DCHECK(t != types::DataType::DATA_TYPE_UNKNOWN);
    children_data_types.push_back(t);
  }

  auto udftype_or_s = compiler_state->registry_info()->GetUDFExecType(func->func_name());
  if (!udftype_or_s.ok()) {
    return func->CreateIRNodeError(udftype_or_s.status().msg());
  }
  switch (udftype_or_s.ConsumeValueOrDie()) {
    case UDFExecType::kUDF: {
      auto data_type_or_s =
          compiler_state->registry_info()->GetUDFDataType(func->func_name(), children_data_types);
      if (!data_type_or_s.status().ok()) {
        return func->CreateIRNodeError(data_type_or_s.status().msg());
      }
      types::DataType data_type = data_type_or_s.ConsumeValueOrDie();
      func->set_func_id(
          compiler_state->GetUDFID(RegistryKey(func->func_name(), children_data_types)));
      func->SetOutputDataType(data_type);
      break;
    }
    case UDFExecType::kUDA: {
      PL_ASSIGN_OR_RETURN(
          types::DataType data_type,
          compiler_state->registry_info()->GetUDADataType(func->func_name(), children_data_types));
      PL_ASSIGN_OR_RETURN(bool can_partial, compiler_state->registry_info()->DoesUDASupportPartial(
                                                func->func_name(), children_data_types));
      func->set_func_id(
          compiler_state->GetUDAID(RegistryKey(func->func_name(), children_data_types)));
      func->SetOutputDataType(data_type);
      func->SetSupportsPartial(can_partial);
      break;
    }
    default: {
      return error::Internal("Unsupported UDFExecType");
    }
  }

  func->SetArgsTypes(children_data_types);
  return true;
}

StatusOr<bool> DataTypeRule::EvaluateColumn(ColumnIR* column) {
  PL_ASSIGN_OR_RETURN(OperatorIR * parent_op, column->ReferencedOperator());
  if (!parent_op->IsRelationInit()) {
    // Missing a relation in parent op is not a failure - the parent op still has to
    // propagate results.
    return false;
  }

  // Get the parent relation and find the column in it.
  PL_RETURN_IF_ERROR(EvaluateColumnFromRelation(column, parent_op->relation()));
  return true;
}

StatusOr<bool> DataTypeRule::EvaluateMetadata(MetadataIR* md) {
  md->ResolveColumnType(md->property()->column_type());
  return true;
}

Status DataTypeRule::EvaluateColumnFromRelation(ColumnIR* column, const Relation& relation) {
  if (!relation.HasColumn(column->col_name())) {
    return column->CreateIRNodeError("Column '$0' not found in parent dataframe",
                                     column->col_name());
  }
  column->ResolveColumnType(relation);
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
