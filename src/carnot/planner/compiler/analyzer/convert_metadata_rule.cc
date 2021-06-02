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

#include <vector>

#include "src/carnot/planner/compiler/analyzer/convert_metadata_rule.h"
#include "src/carnot/planner/compiler/analyzer/data_type_rule.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

Status ConvertMetadataRule::UpdateMetadataContainer(IRNode* container, MetadataIR* metadata,
                                                    ExpressionIR* metadata_expr) const {
  if (Match(container, Func())) {
    auto func = static_cast<FuncIR*>(container);
    for (const auto& [arg_idx, arg] : Enumerate(func->args())) {
      if (arg == metadata) {
        PL_RETURN_IF_ERROR(func->UpdateArg(arg_idx, metadata_expr));
      }
    }
    return Status::OK();
  }
  if (Match(container, Map())) {
    auto map = static_cast<MapIR*>(container);
    for (const auto& expr : map->col_exprs()) {
      if (expr.node == metadata) {
        PL_RETURN_IF_ERROR(map->UpdateColExpr(expr.name, metadata_expr));
      }
    }
    return Status::OK();
  }
  if (Match(container, Filter())) {
    auto filter = static_cast<FilterIR*>(container);
    return filter->SetFilterExpr(metadata_expr);
  }
  return error::Internal("Unsupported IRNode container for metadata: $0", container->DebugString());
}

StatusOr<std::string> ConvertMetadataRule::FindKeyColumn(const Relation& parent_relation,
                                                         MetadataProperty* property,
                                                         IRNode* node_for_error) const {
  DCHECK_NE(property, nullptr);
  for (const std::string& key_col : property->GetKeyColumnReprs()) {
    if (parent_relation.HasColumn(key_col)) {
      return key_col;
    }
  }
  return node_for_error->CreateIRNodeError(
      "Can't resolve metadata because of lack of converting columns in the parent. Need one of "
      "[$0]. Parent relation has columns [$1] available.",
      absl::StrJoin(property->GetKeyColumnReprs(), ","),
      absl::StrJoin(parent_relation.col_names(), ","));
}

StatusOr<bool> ConvertMetadataRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Metadata())) {
    return false;
  }

  auto graph = ir_node->graph();
  auto metadata = static_cast<MetadataIR*>(ir_node);
  auto md_property = metadata->property();
  auto parent_op_idx = metadata->container_op_parent_idx();
  auto column_type = md_property->column_type();
  auto md_type = md_property->metadata_type();

  PL_ASSIGN_OR_RETURN(auto parent, metadata->ReferencedOperator());

  PL_ASSIGN_OR_RETURN(std::string key_column_name,
                      FindKeyColumn(parent->relation(), md_property, ir_node));

  PL_ASSIGN_OR_RETURN(ColumnIR * key_column,
                      graph->CreateNode<ColumnIR>(ir_node->ast(), key_column_name, parent_op_idx));

  PL_ASSIGN_OR_RETURN(std::string func_name, md_property->UDFName(key_column_name));
  PL_ASSIGN_OR_RETURN(
      FuncIR * conversion_func,
      graph->CreateNode<FuncIR>(ir_node->ast(), FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                                std::vector<ExpressionIR*>{key_column}));
  for (int64_t parent_id : graph->dag().ParentsOf(metadata->id())) {
    // For each container node of the metadata expression, update it to point to the
    // new conversion func instead.
    PL_RETURN_IF_ERROR(UpdateMetadataContainer(graph->Get(parent_id), metadata, conversion_func));
  }

  // Manually evaluate the column type, because DataTypeRule will run before this rule.
  PL_ASSIGN_OR_RETURN(auto evaled_col, DataTypeRule::EvaluateColumn(key_column));
  DCHECK(evaled_col);

  PL_ASSIGN_OR_RETURN(auto evaled_func,
                      DataTypeRule::EvaluateFunc(compiler_state_, conversion_func));
  DCHECK(evaled_func);
  DCHECK_EQ(conversion_func->EvaluatedDataType(), column_type)
      << "Expected the parent_relation key column type and metadata property type to match.";
  conversion_func->set_annotations(ExpressionIR::Annotations(md_type));

  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
