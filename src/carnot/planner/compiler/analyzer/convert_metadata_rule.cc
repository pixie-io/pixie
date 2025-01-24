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

#include <memory>
#include <vector>

#include "src/carnot/planner/compiler/analyzer/convert_metadata_rule.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/filter_ir.h"
#include "src/carnot/planner/ir/func_ir.h"
#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/ir/metadata_ir.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

Status ConvertMetadataRule::UpdateMetadataContainer(IRNode* container, MetadataIR* metadata,
                                                    ExpressionIR* metadata_expr) const {
  if (Match(container, Func())) {
    auto func = static_cast<FuncIR*>(container);
    PX_RETURN_IF_ERROR(func->UpdateArg(metadata, metadata_expr));
    return Status::OK();
  }
  if (Match(container, Map())) {
    auto map = static_cast<MapIR*>(container);
    for (const auto& expr : map->col_exprs()) {
      if (expr.node == metadata) {
        PX_RETURN_IF_ERROR(map->UpdateColExpr(expr.name, metadata_expr));
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

StatusOr<std::string> ConvertMetadataRule::FindKeyColumn(std::shared_ptr<TableType> parent_type,
                                                         MetadataProperty* property,
                                                         IRNode* node_for_error) const {
  DCHECK_NE(property, nullptr);
  for (const std::string& key_col : property->GetKeyColumnReprs()) {
    if (parent_type->HasColumn(key_col)) {
      return key_col;
    }
  }
  return node_for_error->CreateIRNodeError(
      "Can't resolve metadata because of lack of converting columns in the parent. Need one of "
      "[$0]. Parent type has columns [$1] available.",
      absl::StrJoin(property->GetKeyColumnReprs(), ","),
      absl::StrJoin(parent_type->ColumnNames(), ","));
}

StatusOr<FuncIR*> AddUPIDToPodNameFallback(std::shared_ptr<TableType> parent_type, IRNode* ir_node,
                                           IR* graph) {
  if (!parent_type->HasColumn("local_addr") || !parent_type->HasColumn("time_")) {
    return error::NotFound("Parent type does not have required columns for fallback conversion.");
  }
  PX_ASSIGN_OR_RETURN(auto upid_column, graph->CreateNode<ColumnIR>(ir_node->ast(), "upid", 0));
  PX_ASSIGN_OR_RETURN(auto local_addr_column,
                      graph->CreateNode<ColumnIR>(ir_node->ast(), "local_addr", 0));
  PX_ASSIGN_OR_RETURN(auto time_column, graph->CreateNode<ColumnIR>(ir_node->ast(), "time_", 0));
  return graph->CreateNode<FuncIR>(
      ir_node->ast(),
      FuncIR::Op{FuncIR::Opcode::non_op, "", "_upid_to_podname_local_addr_fallback"},
      std::vector<ExpressionIR*>{upid_column, local_addr_column, time_column});
}

StatusOr<FuncIR*> AddBackupConversions(std::shared_ptr<TableType> parent_type,
                                       std::string func_name, IRNode* ir_node, IR* graph) {
  if (absl::StrContains(func_name, "upid_to_pod_name")) {
    return AddUPIDToPodNameFallback(parent_type, ir_node, graph);
  }
  return error::NotFound("No backup conversion function available for $0", func_name);
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

  PX_ASSIGN_OR_RETURN(auto parent, metadata->ReferencedOperator());
  PX_ASSIGN_OR_RETURN(auto containing_ops, metadata->ContainingOperators());

  auto resolved_table_type = parent->resolved_table_type();
  PX_ASSIGN_OR_RETURN(std::string key_column_name,
                      FindKeyColumn(resolved_table_type, md_property, ir_node));

  PX_ASSIGN_OR_RETURN(std::string func_name, md_property->UDFName(key_column_name));

  // TODO(ddelnano): Until the short lived process issue (gh#1638) is resolved, use a
  // conversion function that uses local_addr for pod lookups when the upid based default
  // (upid_to_pod_name) fails.
  auto backup_conversion_func =
      AddBackupConversions(resolved_table_type, func_name, ir_node, graph);
  FuncIR* conversion_func = nullptr;
  if (backup_conversion_func.ok()) {
    conversion_func = backup_conversion_func.ValueOrDie();
  }

  if (conversion_func == nullptr) {
    PX_ASSIGN_OR_RETURN(ColumnIR * key_column, graph->CreateNode<ColumnIR>(
                                                   ir_node->ast(), key_column_name, parent_op_idx));
    PX_ASSIGN_OR_RETURN(
        conversion_func,
        graph->CreateNode<FuncIR>(ir_node->ast(), FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                                  std::vector<ExpressionIR*>{key_column}));
  }
  for (int64_t parent_id : graph->dag().ParentsOf(metadata->id())) {
    // For each container node of the metadata expression, update it to point to the
    // new conversion func instead.
    PX_RETURN_IF_ERROR(UpdateMetadataContainer(graph->Get(parent_id), metadata, conversion_func));
  }

  // Propagate type changes from the new conversion_func.
  PX_RETURN_IF_ERROR(PropagateTypeChangesFromNode(graph, conversion_func, compiler_state_));

  DCHECK_EQ(conversion_func->EvaluatedDataType(), column_type)
      << "Expected the parent key column type and metadata property type to match.";
  conversion_func->set_annotations(ExpressionIR::Annotations(md_type));

  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
