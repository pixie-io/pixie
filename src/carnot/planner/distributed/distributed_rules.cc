#include "src/carnot/planner/distributed/distributed_rules.h"

#include <utility>

#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace pl {
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

StatusOr<bool> HasScalarFuncExecutor(CompilerState* compiler_state, IRNode* node,
                                     udfspb::UDFSourceExecutor executor) {
  if (Match(node, Operator())) {
    // Get the operator's child funcs, and call this function on them to check if their UDFs can
    // successfully execute based on the target execution type.
    auto op_children = node->graph()->dag().DependenciesOf(node->id());
    for (const auto& op_child_id : op_children) {
      auto child_node = node->graph()->Get(op_child_id);
      if (Match(child_node, Func())) {
        PL_ASSIGN_OR_RETURN(auto res, HasScalarFuncExecutor(compiler_state, child_node, executor));
        if (res) {
          return res;
        }
      }
    }
  }

  // If a node is not an operator nor a func, then there are no scalar funcs to check.
  if (!Match(node, Func())) {
    return false;
  }
  auto func = static_cast<FuncIR*>(node);

  // Get the types of the children of this function.
  std::vector<types::DataType> children_data_types;
  for (const auto& arg : func->args()) {
    PL_ASSIGN_OR_RETURN(auto res, HasScalarFuncExecutor(compiler_state, arg, executor));
    if (res) {
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
    return false;
  }

  PL_ASSIGN_OR_RETURN(auto res, compiler_state->registry_info()->GetUDFSourceExecutor(
                                    func->func_name(), children_data_types));
  return executor == res;
}

// Get new locations for the input limit node.
// A single limit may be cloned and pushed up to multiple branches.
StatusOr<absl::flat_hash_set<OperatorIR*>> LimitPushdownRule::NewLimitParents(
    OperatorIR* current_node) {
  // Maps we can simply push up the chain.
  if (Match(current_node, Map())) {
    DCHECK_EQ(1, current_node->parents().size());
    // Don't push a Limit earlier than a PEM-only Map, because we need to ensure that after
    // splitting on Limit nodes, we don't end up with a PEM-only map on the Kelvin side of
    // the distributed plan.
    PL_ASSIGN_OR_RETURN(
        auto has_pem_only_udf,
        HasScalarFuncExecutor(compiler_state_, current_node, udfspb::UDFSourceExecutor::UDF_PEM));
    if (!has_pem_only_udf) {
      return NewLimitParents(current_node->parents()[0]);
    }
  }
  // Unions will need at most N records from each source.
  if (Match(current_node, Union())) {
    absl::flat_hash_set<OperatorIR*> results;
    // We want 1 Limit node after each union, and one before
    // each of its branches.
    results.insert(current_node);

    for (OperatorIR* parent : current_node->parents()) {
      PL_ASSIGN_OR_RETURN(auto parent_results, NewLimitParents(parent));
      for (OperatorIR* parent_result : parent_results) {
        results.insert(parent_result);
      }
    }
    return results;
  }
  return absl::flat_hash_set<OperatorIR*>{current_node};
}

StatusOr<bool> LimitPushdownRule::Apply(IRNode* ir_node) {
  if (!Match(ir_node, Limit())) {
    return false;
  }

  auto graph = ir_node->graph();

  LimitIR* limit = static_cast<LimitIR*>(ir_node);
  DCHECK_EQ(1, limit->parents().size());
  OperatorIR* limit_parent = limit->parents()[0];

  PL_ASSIGN_OR_RETURN(auto new_parents, NewLimitParents(limit_parent));
  // If we don't push the limit up at all, just return.
  if (new_parents.size() == 1 && new_parents.find(limit_parent) != new_parents.end()) {
    return false;
  }

  // Remove the limit from its previous location.
  for (OperatorIR* child : limit->Children()) {
    PL_RETURN_IF_ERROR(child->ReplaceParent(limit, limit_parent));
  }
  PL_RETURN_IF_ERROR(limit->RemoveParent(limit_parent));

  // Add the limit to its new location(s).
  for (OperatorIR* new_parent : new_parents) {
    PL_ASSIGN_OR_RETURN(LimitIR * new_limit, graph->CopyNode(limit));
    // The parent's children should now be the children of the limit.
    for (OperatorIR* former_child : new_parent->Children()) {
      PL_RETURN_IF_ERROR(former_child->ReplaceParent(new_parent, new_limit));
    }
    // The limit should now be a child of the parent.
    PL_RETURN_IF_ERROR(new_limit->AddParent(new_parent));
    // Ensure we inherit the relation of the parent.
    PL_RETURN_IF_ERROR(new_limit->SetRelation(new_parent->relation()));
  }

  PL_RETURN_IF_ERROR(graph->DeleteNode(limit->id()));
  return true;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
