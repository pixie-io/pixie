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

namespace {
std::string GetUniquePodNameCol(std::shared_ptr<TableType> parent_type,
                                absl::flat_hash_set<std::string>* used_column_names) {
  auto col_name_counter = 0;
  do {
    auto new_col = absl::StrCat("pod_name_", col_name_counter++);
    if (!used_column_names->contains(new_col) && !parent_type->HasColumn(new_col)) {
      used_column_names->insert(new_col);
      return new_col;
    }
  } while (true);
}
}  // namespace

Status ConvertMetadataRule::AddPodNameConversionMapsWithFallback(
    IR* graph, IRNode* container, ExpressionIR* metadata_expr, ExpressionIR* fallback_expr,
    const std::pair<std::string, std::string>& col_names) const {
  if (Match(container, Func())) {
    for (int64_t parent_id : graph->dag().ParentsOf(container->id())) {
      PX_RETURN_IF_ERROR(AddPodNameConversionMapsWithFallback(
          graph, graph->Get(parent_id), metadata_expr, fallback_expr, col_names));
    }
  } else if (Match(container, Operator())) {
    auto container_op = static_cast<OperatorIR*>(container);
    for (auto parent_op : container_op->parents()) {
      auto metadata_col_expr = ColumnExpression(col_names.first, metadata_expr);
      auto fallback_col_expr = ColumnExpression(col_names.second, fallback_expr);
      PX_ASSIGN_OR_RETURN(
          auto md_map_ir,
          graph->CreateNode<MapIR>(container->ast(), parent_op,
                                   std::vector<ColumnExpression>{metadata_col_expr}, true));
      PX_ASSIGN_OR_RETURN(
          auto fallback_md_map_ir,
          graph->CreateNode<MapIR>(container->ast(), static_cast<OperatorIR*>(md_map_ir),
                                   std::vector<ColumnExpression>{fallback_col_expr}, true));
      PX_RETURN_IF_ERROR(container_op->ReplaceParent(parent_op, fallback_md_map_ir));

      for (auto child : parent_op->Children()) {
        if (child == md_map_ir) {
          continue;
        }
        PX_RETURN_IF_ERROR(child->ReplaceParent(parent_op, md_map_ir));
      }

      PX_RETURN_IF_ERROR(PropagateTypeChangesFromNode(graph, md_map_ir, compiler_state_));
      PX_RETURN_IF_ERROR(PropagateTypeChangesFromNode(graph, fallback_md_map_ir, compiler_state_));
    }
  }

  return Status::OK();
}

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

bool CheckBackupConversionAvailable(std::shared_ptr<TableType> parent_type,
                                    const std::string& func_name) {
  return parent_type->HasColumn("time_") && parent_type->HasColumn("local_addr") &&
         func_name == "upid_to_pod_name";
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

  PX_ASSIGN_OR_RETURN(ColumnIR * key_column,
                      graph->CreateNode<ColumnIR>(ir_node->ast(), key_column_name, parent_op_idx));

  PX_ASSIGN_OR_RETURN(std::string func_name, md_property->UDFName(key_column_name));
  PX_ASSIGN_OR_RETURN(
      FuncIR * conversion_func,
      graph->CreateNode<FuncIR>(ir_node->ast(), FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                                std::vector<ExpressionIR*>{key_column}));
  FuncIR* orig_conversion_func = conversion_func;
  ExpressionIR* conversion_expr = static_cast<ExpressionIR*>(conversion_func);

  // TODO(ddelnano): Until the short lived process issue (gh#1638) is resolved, add a fallback
  // conversion function that uses local_addr for pod lookups when the upid based default
  // (upid_to_pod_name) fails. This turns the `df.ctx["pod"]` lookup into the following pseudo code:
  //
  //  fallback = px.pod_id_to_pod_name(px.ip_to_pod_id(df.ctx["local_addr"]))
  //  df.pod = px.select(px.upid_to_pod_name(df.upid) == "", fallback, px.upid_to_pod_name(df.upid))
  FuncIR* backup_conversion_func = nullptr;
  auto backup_conversion_available = CheckBackupConversionAvailable(resolved_table_type, func_name);
  std::pair<std::string, std::string> col_names;
  if (backup_conversion_available) {
    absl::flat_hash_set<std::string> used_column_names;
    col_names = std::make_pair(GetUniquePodNameCol(resolved_table_type, &used_column_names),
                               GetUniquePodNameCol(resolved_table_type, &used_column_names));
    PX_ASSIGN_OR_RETURN(ColumnIR * local_addr_col,
                        graph->CreateNode<ColumnIR>(ir_node->ast(), "local_addr", parent_op_idx));
    PX_ASSIGN_OR_RETURN(ColumnIR * time_col,
                        graph->CreateNode<ColumnIR>(ir_node->ast(), "time_", parent_op_idx));
    PX_ASSIGN_OR_RETURN(
        ColumnIR * md_expr_col,
        graph->CreateNode<ColumnIR>(ir_node->ast(), col_names.first, parent_op_idx));
    PX_ASSIGN_OR_RETURN(
        FuncIR * ip_conversion_func,
        graph->CreateNode<FuncIR>(ir_node->ast(),
                                  FuncIR::Op{FuncIR::Opcode::non_op, "", "_ip_to_pod_id_pem_exec"},
                                  std::vector<ExpressionIR*>{local_addr_col, time_col}));

    // This doesn't need to have a "pem exec" equivalent function as long as the metadata
    // annotation is set as done below
    PX_ASSIGN_OR_RETURN(
        backup_conversion_func,
        graph->CreateNode<FuncIR>(
            ir_node->ast(), FuncIR::Op{FuncIR::Opcode::non_op, "", "pod_id_to_pod_name"},
            std::vector<ExpressionIR*>{static_cast<ExpressionIR*>(ip_conversion_func)}));

    backup_conversion_func->set_annotations(ExpressionIR::Annotations(md_type));

    PX_ASSIGN_OR_RETURN(ExpressionIR * empty_string,
                        graph->CreateNode<StringIR>(ir_node->ast(), ""));
    PX_ASSIGN_OR_RETURN(
        FuncIR * select_expr,
        graph->CreateNode<FuncIR>(
            ir_node->ast(), FuncIR::Op{FuncIR::Opcode::eq, "==", "equal"},
            std::vector<ExpressionIR*>{static_cast<ExpressionIR*>(md_expr_col), empty_string}));
    PX_ASSIGN_OR_RETURN(auto duplicate_md_expr_col, graph->CopyNode<ColumnIR>(md_expr_col));
    PX_ASSIGN_OR_RETURN(
        FuncIR * select_func,
        graph->CreateNode<FuncIR>(
            ir_node->ast(), FuncIR::Op{FuncIR::Opcode::non_op, "", "select"},
            std::vector<ExpressionIR*>{static_cast<ExpressionIR*>(select_expr),
                                       backup_conversion_func, duplicate_md_expr_col}));

    conversion_func = select_func;
    PX_ASSIGN_OR_RETURN(conversion_expr, graph->CreateNode<ColumnIR>(
                                             ir_node->ast(), col_names.second, parent_op_idx));
  }

  for (int64_t parent_id : graph->dag().ParentsOf(metadata->id())) {
    // For each container node of the metadata expression, update it to point to the
    // new conversion func instead.
    auto container = graph->Get(parent_id);
    PX_RETURN_IF_ERROR(UpdateMetadataContainer(container, metadata, conversion_expr));
    if (backup_conversion_available) {
      PX_RETURN_IF_ERROR(AddPodNameConversionMapsWithFallback(
          graph, container, orig_conversion_func, conversion_func, col_names));
    }
  }

  // Propagate type changes from the new conversion_func.
  PX_RETURN_IF_ERROR(PropagateTypeChangesFromNode(graph, conversion_func, compiler_state_));

  DCHECK_EQ(conversion_func->EvaluatedDataType(), column_type)
      << "Expected the parent key column type and metadata property type to match.";
  orig_conversion_func->set_annotations(ExpressionIR::Annotations(md_type));
  return true;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
