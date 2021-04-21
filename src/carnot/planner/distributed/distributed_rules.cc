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

#include <utility>

#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/carnot/planner/distributed/executor_utils.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

PruneUnavailableSourcesRule::PruneUnavailableSourcesRule(
    int64_t agent_id, const distributedpb::CarnotInfo& carnot_info,
    const SchemaToAgentsMap& schema_map)
    : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
      agent_id_(agent_id),
      carnot_info_(carnot_info),
      schema_map_(schema_map) {}

StatusOr<bool> PruneUnavailableSourcesRule::Apply(IRNode* node) {
  if (Match(node, SourceOperator())) {
    return RemoveSourceIfNotNecessary(static_cast<OperatorIR*>(node));
  }
  return false;
}

StatusOr<bool> PruneUnavailableSourcesRule::RemoveSourceIfNotNecessary(OperatorIR* source_op) {
  DCHECK(source_op->IsSource());
  if (Match(source_op, MemorySource())) {
    return MaybePruneMemorySource(static_cast<MemorySourceIR*>(source_op));
  } else if (Match(source_op, UDTFSource())) {
    return MaybePruneUDTFSource(static_cast<UDTFSourceIR*>(source_op));
  }
  return false;
}

Status DeleteSourceAndChildren(OperatorIR* source_op) {
  DCHECK(source_op->IsSource());
  // TODO(PL-1468) figure out how to delete the Join parents.
  return source_op->graph()->DeleteOrphansInSubtree(source_op->id());
}

StatusOr<bool> PruneUnavailableSourcesRule::MaybePruneMemorySource(MemorySourceIR* mem_src) {
  if (!AgentSupportsMemorySources()) {
    PL_RETURN_IF_ERROR(DeleteSourceAndChildren(mem_src));
    return true;
  }

  if (!AgentHasTable(mem_src->table_name())) {
    PL_RETURN_IF_ERROR(DeleteSourceAndChildren(mem_src));
    return true;
  }
  return false;
}

bool PruneUnavailableSourcesRule::AgentSupportsMemorySources() {
  return carnot_info_.has_data_store() && !carnot_info_.has_grpc_server() &&
         carnot_info_.processes_data();
}

bool PruneUnavailableSourcesRule::AgentHasTable(std::string table_name) {
  auto schema_iter = schema_map_.find(table_name);
  return schema_iter != schema_map_.end() && schema_iter->second.contains(agent_id_);
}

StatusOr<bool> PruneUnavailableSourcesRule::MaybePruneUDTFSource(UDTFSourceIR* udtf_src) {
  // If the Agent does execute UDTF and the the UDTF Matches features, then we do not prune.
  if (AgentExecutesUDTF(udtf_src, carnot_info_) && UDTFMatchesFilters(udtf_src, carnot_info_)) {
    return false;
  }
  // Otherwise, we remove the source.
  PL_RETURN_IF_ERROR(DeleteSourceAndChildren(udtf_src));
  return true;
}

bool PruneUnavailableSourcesRule::IsPEM(const distributedpb::CarnotInfo& carnot_info) {
  return carnot_info.has_data_store() && carnot_info.processes_data() &&
         !carnot_info.has_grpc_server();
}

bool PruneUnavailableSourcesRule::IsKelvin(const distributedpb::CarnotInfo& carnot_info) {
  return carnot_info.has_grpc_server() && carnot_info.processes_data();
}

bool PruneUnavailableSourcesRule::AgentExecutesUDTF(UDTFSourceIR* source,
                                                    const distributedpb::CarnotInfo& carnot_info) {
  const auto& udtf_spec = source->udtf_spec();
  switch (udtf_spec.executor()) {
    case udfspb::UDTF_ALL_AGENTS:
      return true;
    case udfspb::UDTF_ALL_KELVIN:
      DCHECK(false) << "UDTF for all kelvin not yet supported" << udtf_spec.DebugString();
      return false;
    case udfspb::UDTF_ALL_PEM:
      return IsPEM(carnot_info);
    case udfspb::UDTF_SUBSET_PEM:
      return IsPEM(carnot_info);
    case udfspb::UDTF_SUBSET_KELVIN:
      return IsKelvin(carnot_info);
    case udfspb::UDTF_ONE_KELVIN:
      return IsKelvin(carnot_info);
    default: {
      DCHECK(false) << "UDTF spec improperly specified" << udtf_spec.DebugString();
      return false;
    }
  }
}

bool PruneUnavailableSourcesRule::UDTFMatchesFilters(UDTFSourceIR* source,
                                                     const distributedpb::CarnotInfo& carnot_info) {
  const auto& udtf_spec = source->udtf_spec();
  for (const auto& [idx, arg] : Enumerate(udtf_spec.args())) {
    DataIR* data = source->arg_values()[idx];

    switch (arg.semantic_type()) {
      // We do not filter on None types.
      case types::ST_NONE: {
        continue;
      }
      // UPID arg means we should check whether the Carnot instance ASID matches the UPID's ASID.
      case types::ST_UPID: {
        // These conditions should already be checked in pl_module.
        DCHECK_EQ(arg.arg_type(), types::UINT128);
        DCHECK_EQ(data->type(), IRNodeType::kUInt128);
        UInt128IR* upid_uint128 = static_cast<UInt128IR*>(data);
        // Convert string to UPID.
        // Get the ASID out of the UPID and compare it to the ASID of the Agent.
        if (md::UPID(upid_uint128->val()).asid() != carnot_info.asid()) {
          return false;
        }
        break;
      }
      case types::ST_AGENT_UID: {
        DCHECK_EQ(arg.arg_type(), types::STRING);
        DCHECK_EQ(data->type(), IRNodeType::kString);
        StringIR* str = static_cast<StringIR*>(data);
        auto uuid = ParseUUID(carnot_info.agent_id()).ConsumeValueOrDie();
        if (uuid.str() != str->str()) {
          return false;
        }
        continue;
      }
      default: {
        CHECK(false) << absl::Substitute("Argument spec for UDTF '$0' set improperly for '$1'",
                                         udtf_spec.name(), arg.name());
        break;
      }
    }
  }
  return true;
}

StatusOr<bool> DistributedPruneUnavailableSourcesRule::Apply(
    distributed::CarnotInstance* carnot_instance) {
  PruneUnavailableSourcesRule rule(carnot_instance->id(), carnot_instance->carnot_info(),
                                   schema_map_);
  return rule.Execute(carnot_instance->plan());
}

StatusOr<bool> PruneEmptyPlansRule::Apply(distributed::CarnotInstance* node) {
  if (node->plan()->FindNodesThatMatch(Operator()).size() > 0) {
    return false;
  }
  PL_RETURN_IF_ERROR(node->distributed_plan()->DeleteNode(node->id()));
  return true;
}

StatusOr<SchemaToAgentsMap> LoadSchemaMap(
    const distributedpb::DistributedState& distributed_state,
    const absl::flat_hash_map<sole::uuid, int64_t>& uuid_to_id_map) {
  SchemaToAgentsMap agent_schema_map;
  for (const auto& schema : distributed_state.schema_info()) {
    absl::flat_hash_set<int64_t> agent_ids;
    for (const auto& uid_pb : schema.agent_list()) {
      PL_ASSIGN_OR_RETURN(sole::uuid uuid, ParseUUID(uid_pb));
      if (!uuid_to_id_map.contains(uuid)) {
        VLOG(1) << absl::Substitute("UUID $0 not found in agent_id_to_plan_id map", uuid.str());
        continue;
      }
      agent_ids.insert(uuid_to_id_map.find(uuid)->second);
    }
    agent_schema_map[schema.name()] = std::move(agent_ids);
  }
  return agent_schema_map;
}

StatusOr<bool> DistributedAnnotateAbortableSrcsForLimitsRule::Apply(
    distributed::CarnotInstance* carnot_instance) {
  AnnotateAbortableSrcsForLimitsRule rule;
  return rule.Execute(carnot_instance->plan());
}

StatusOr<bool> AnnotateAbortableSrcsForLimitsRule::Apply(IRNode* node) {
  if (!Match(node, Limit())) {
    return false;
  }
  IR* graph = node->graph();
  auto limit = static_cast<LimitIR*>(node);

  plan::DAG dag_copy = graph->dag();
  dag_copy.DeleteNode(limit->id());
  auto src_nodes = graph->FindNodesThatMatch(Source());
  auto sink_nodes = graph->FindNodesThatMatch(Sink());
  bool changed = false;
  for (const auto& src : src_nodes) {
    auto transitive_deps = dag_copy.TransitiveDepsFrom(src->id());
    bool is_abortable = true;
    for (const auto sink : sink_nodes) {
      if (transitive_deps.find(sink->id()) != transitive_deps.end()) {
        // If there is a sink in the transitive children of the source node then this source node
        // is not abortable from this limit node.
        is_abortable = false;
        break;
      }
    }
    if (is_abortable) {
      limit->AddAbortableSource(src->id());
      changed = true;
    }
  }
  return changed;
}

// Helper struct for the scalar UDF checker rules, ScalarUDFsRunOnPEMRule/etc.
struct UDFCheckerResult {
  bool success;
  std::string bad_udf_name;
  static UDFCheckerResult Success() {
    UDFCheckerResult res;
    res.success = true;
    return res;
  }
  static UDFCheckerResult Fail(const std::string& udf_name) {
    UDFCheckerResult res;
    res.success = false;
    res.bad_udf_name = udf_name;
    return res;
  }
};

// Helper func for the scalar UDF checker rules, ScalarUDFsRunOnPEMRule/etc.
StatusOr<UDFCheckerResult> CheckScalarFuncExecutor(
    CompilerState* compiler_state, IRNode* node,
    absl::flat_hash_set<udfspb::UDFSourceExecutor> valid) {
  if (Match(node, Operator())) {
    // Get the operator's child funcs, and call this function on them to check if their UDFs can
    // successfully execute based on the target execution type.
    auto op_children = node->graph()->dag().DependenciesOf(node->id());
    for (const auto& op_child_id : op_children) {
      auto child_node = node->graph()->Get(op_child_id);
      if (Match(child_node, Func())) {
        PL_ASSIGN_OR_RETURN(auto res, CheckScalarFuncExecutor(compiler_state, child_node, valid));
        if (!res.success) {
          return res;
        }
      }
    }
  }

  // If a node is not an operator nor a func, then there are no scalar funcs to check.
  if (!Match(node, Func())) {
    return UDFCheckerResult::Success();
  }
  auto func = static_cast<FuncIR*>(node);

  // Get the types of the children of this function.
  std::vector<types::DataType> children_data_types;
  for (const auto& arg : func->args()) {
    PL_ASSIGN_OR_RETURN(auto res, CheckScalarFuncExecutor(compiler_state, arg, valid));
    if (!res.success) {
      return res;
    }
    types::DataType t = arg->EvaluatedDataType();
    if (t == types::DataType::DATA_TYPE_UNKNOWN) {
      return error::Internal("Type of arg $0 to func '$1' is not resolved", arg->DebugString(),
                             func->func_name());
    }
    children_data_types.push_back(t);
  }

  PL_ASSIGN_OR_RETURN(auto udf_type,
                      compiler_state->registry_info()->GetUDFExecType(func->func_name()));
  if (udf_type != UDFExecType::kUDF) {
    return UDFCheckerResult::Success();
  }

  PL_ASSIGN_OR_RETURN(auto executor, compiler_state->registry_info()->GetUDFSourceExecutor(
                                         func->func_name(), children_data_types));
  if (!valid.contains(executor)) {
    return UDFCheckerResult::Fail(func->func_name());
  }
  return UDFCheckerResult::Success();
}

const absl::flat_hash_set<udfspb::UDFSourceExecutor> kValidPEMUDFExecutors{
    udfspb::UDFSourceExecutor::UDF_ALL, udfspb::UDFSourceExecutor::UDF_PEM};
const absl::flat_hash_set<udfspb::UDFSourceExecutor> kValidKelvinUDFExecutors{
    udfspb::UDFSourceExecutor::UDF_ALL, udfspb::UDFSourceExecutor::UDF_KELVIN};

StatusOr<bool> ScalarUDFsRunOnPEMRule::OperatorUDFsRunOnPEM(CompilerState* compiler_state,
                                                            OperatorIR* op) {
  PL_ASSIGN_OR_RETURN(auto res, CheckScalarFuncExecutor(compiler_state, op, kValidPEMUDFExecutors));
  return res.success;
}

StatusOr<bool> ScalarUDFsRunOnPEMRule::Apply(IRNode* node) {
  if (!Match(node, Operator())) {
    return false;
  }
  PL_ASSIGN_OR_RETURN(auto res,
                      CheckScalarFuncExecutor(compiler_state_, node, kValidPEMUDFExecutors));
  if (!res.success) {
    return node->CreateIRNodeError(
        "UDF '$0' must execute after blocking nodes such as limit, agg, and join.",
        res.bad_udf_name);
  }
  return false;
}

StatusOr<bool> ScalarUDFsRunOnKelvinRule::OperatorUDFsRunOnKelvin(CompilerState* compiler_state,
                                                                  OperatorIR* op) {
  PL_ASSIGN_OR_RETURN(auto res,
                      CheckScalarFuncExecutor(compiler_state, op, kValidKelvinUDFExecutors));
  return res.success;
}

StatusOr<bool> ScalarUDFsRunOnKelvinRule::Apply(IRNode* node) {
  if (!Match(node, Operator())) {
    return false;
  }
  PL_ASSIGN_OR_RETURN(auto res,
                      CheckScalarFuncExecutor(compiler_state_, node, kValidKelvinUDFExecutors));
  if (!res.success) {
    return node->CreateIRNodeError(
        "UDF '$0' must execute before blocking nodes such as limit, agg, and join.",
        res.bad_udf_name);
  }
  return false;
}

std::string GetUniqueOutputName(FuncIR* input_expr,
                                const absl::flat_hash_set<std::string>& used_column_names) {
  std::string output_name;
  auto idx = 0;
  while (used_column_names.contains(
      output_name = absl::Substitute("$0_$1", input_expr->func_name(), idx++))) {
    // Keep incrementing idx until we get a unique name.
  }
  return output_name;
}

// If `expr` or one of its children is a PEM-only UDF, move it to be an output
// of the PEM-only map.
StatusOr<absl::flat_hash_set<std::string>>
SplitPEMandKelvinOnlyUDFOperatorRule::OptionallyUpdateExpression(
    IRNode* expr_parent, ExpressionIR* expr, MapIR* pem_only_map,
    const absl::flat_hash_set<std::string>& used_column_names) {
  if (!Match(expr, Func())) {
    return absl::flat_hash_set<std::string>({});
  }

  absl::flat_hash_set<std::string> new_col_names;

  auto graph = expr->graph();
  auto func = static_cast<FuncIR*>(expr);

  // Check the root expression first.
  // In the event both this function and its child are both PEM-only UDFs, we should
  // move them both together, rather than moving the child then this parent after.
  PL_ASSIGN_OR_RETURN(
      auto is_scalar_func_executor,
      IsFuncWithExecutor(compiler_state_, expr, udfspb::UDFSourceExecutor::UDF_PEM));
  if (!is_scalar_func_executor) {
    // Even if this func itself isn't a PEM-only UDF, its children still might be.
    // Optionally update all of the child expressions of this expression.
    for (ExpressionIR* arg : func->args()) {
      PL_ASSIGN_OR_RETURN(auto arg_col_names,
                          OptionallyUpdateExpression(func, arg, pem_only_map, used_column_names));
      new_col_names.insert(arg_col_names.begin(), arg_col_names.end());
    }
    return new_col_names;
  }

  // Create the column that will replace the expression in the operator we are splitting.
  auto output_col_name = GetUniqueOutputName(func, used_column_names);
  new_col_names.insert(output_col_name);
  PL_ASSIGN_OR_RETURN(auto input_col, graph->CreateNode<ColumnIR>(expr->ast(), output_col_name,
                                                                  /*parent_op_idx*/ 0));
  // This column should have the same type as the expression, since it's just a projection.
  input_col->ResolveColumnType(expr->EvaluatedDataType());

  // Add the PEM-only expression to the PEM-only map.
  // It will get deleted from its original parent next.
  auto col_expr = ColumnExpression(output_col_name, expr);
  PL_RETURN_IF_ERROR(pem_only_map->AddColExpr(col_expr));

  // Update the original expression's parent to point to the new column in the PEM-only map.
  // We may want to refactor this logic into a utility for updating Operator expressions,
  // so that we don't have to match operator type in every place that we want to replace
  // expression with a new expression.
  if (Match(expr_parent, Filter())) {
    auto filter = static_cast<FilterIR*>(expr_parent);
    PL_RETURN_IF_ERROR(filter->SetFilterExpr(input_col));
  } else if (Match(expr_parent, Map())) {
    auto map = static_cast<MapIR*>(expr_parent);
    PL_RETURN_IF_ERROR(map->UpdateColExpr(expr, input_col));
  } else if (Match(expr_parent, Func())) {
    auto func = static_cast<FuncIR*>(expr_parent);
    PL_RETURN_IF_ERROR(func->UpdateArg(expr, input_col));
  } else {
    return error::Internal("Unexpected parent expression type: $0", expr_parent->type_string());
  }
  return new_col_names;
}

StatusOr<bool> SplitPEMandKelvinOnlyUDFOperatorRule::Apply(IRNode* node) {
  if (!Match(node, Map()) && !Match(node, Filter())) {
    return false;
  }

  PL_ASSIGN_OR_RETURN(
      auto has_pem_only_udf,
      HasFuncWithExecutor(compiler_state_, node, udfspb::UDFSourceExecutor::UDF_PEM));
  if (!has_pem_only_udf) {
    return false;
  }

  auto graph = node->graph();
  auto op = static_cast<OperatorIR*>(node);
  if (op->parents().size() != 1) {
    return op->CreateIRNodeError("Operator unexpectedly has $0 parents, expected 1",
                                 op->parents().size());
  }
  auto parent = op->parents()[0];
  auto parent_relation = parent->relation();

  // Collect the expression(s) to optionally modify.
  std::vector<ExpressionIR*> operator_expressions;
  if (Match(op, Map())) {
    auto map = static_cast<MapIR*>(op);
    for (const auto& expr : map->col_exprs()) {
      operator_expressions.push_back(expr.node);
    }
  } else if (Match(op, Filter())) {
    auto filter = static_cast<FilterIR*>(op);
    operator_expressions.push_back(filter->filter_expr());
  } else {
    return op->CreateIRNodeError("Unexpected operator type, expected Map or Filter");
  }

  // Create a new Map node to handle all of the PEM-only expressions.
  // It will become the parent of the current operator.
  PL_ASSIGN_OR_RETURN(MapIR * pem_map,
                      graph->CreateNode<MapIR>(node->ast(), parent, ColExpressionVector({}),
                                               /* keep_input_columns */ false));
  // Optionally update each expression.
  // If it contains a PEM-only UDF, we will split it into the new map node we are creating.
  // If it doesn't contain a PEM-only UDF, we will not modify it.
  // Also, keep track of the column names the operators are using so we don't autogenerate
  // a new column with a name collision with an existing column.
  auto parent_col_names = parent_relation.col_names();
  absl::flat_hash_set<std::string> used_column_names(parent_col_names.begin(),
                                                     parent_col_names.end());
  for (ExpressionIR* operator_expression : operator_expressions) {
    PL_ASSIGN_OR_RETURN(auto new_col_names, OptionallyUpdateExpression(op, operator_expression,
                                                                       pem_map, used_column_names));
    used_column_names.insert(new_col_names.begin(), new_col_names.end());
  }

  // PEM-only map must contain all of the input columns to the operator `op`.
  // We do this last, because some of those required inputs may no longer be required.
  PL_ASSIGN_OR_RETURN(auto required_inputs_per_parent, op->RequiredInputColumns());
  if (required_inputs_per_parent.size() != 1) {
    return op->CreateIRNodeError("Operator unexpectedly has $0 parents, expected 1",
                                 op->parents().size());
  }
  for (const auto& required_input_col : required_inputs_per_parent[0]) {
    // If a required input column is one we just generated from a PEM-only function,
    // no need to add a column projection for it to the PEM map.
    if (!parent_relation.HasColumn(required_input_col)) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(auto col_node, graph->CreateNode<ColumnIR>(op->ast(), required_input_col,
                                                                   /*parent_op_idx*/ 0));
    col_node->ResolveColumnType(parent_relation);
    PL_RETURN_IF_ERROR(pem_map->AddColExpr(ColumnExpression(required_input_col, col_node)));
  }

  // Update the relation of the PEM-only map.
  // The relation of the parent should be unchanged, since it is just a reassignment
  // of the same output value.
  PL_RETURN_IF_ERROR(pem_map->SetRelationFromExprs());
  PL_RETURN_IF_ERROR(op->ReplaceParent(parent, pem_map));
  return true;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
