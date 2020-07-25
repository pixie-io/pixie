#include "src/carnot/planner/distributed/distributed_rules.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

PruneUnavailableSourcesRule::PruneUnavailableSourcesRule(distributedpb::CarnotInfo carnot_info,
                                                         const SchemaMap& schema_map)
    : Rule(nullptr), carnot_info_(carnot_info), schema_map_(schema_map) {
  agent_id_ = ParseUUID(carnot_info_.agent_id()).ConsumeValueOrDie();
}

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
  if (!schema_map_.contains(table_name)) {
    return false;
  }
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
  PruneUnavailableSourcesRule rule(carnot_instance->carnot_info(), schema_map_);
  return rule.Execute(carnot_instance->plan());
}

StatusOr<bool> PruneEmptyPlansRule::Apply(distributed::CarnotInstance* node) {
  if (node->plan()->FindNodesThatMatch(Operator()).size() > 0) {
    return false;
  }
  PL_RETURN_IF_ERROR(node->distributed_plan()->DeleteNode(node->id()));
  return true;
}

StatusOr<SchemaMap> LoadSchemaMap(const distributedpb::DistributedState& distributed_state) {
  SchemaMap agent_schema_map;
  for (const auto& schema : distributed_state.schema_info()) {
    absl::flat_hash_set<sole::uuid> agent_ids;
    for (const auto& uid_pb : schema.agent_list()) {
      PL_ASSIGN_OR_RETURN(sole::uuid uuid, ParseUUID(uid_pb));
      agent_ids.insert(uuid);
    }
    agent_schema_map[schema.name()] = agent_ids;
  }
  return agent_schema_map;
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
