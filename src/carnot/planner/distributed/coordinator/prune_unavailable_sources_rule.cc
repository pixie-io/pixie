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

#include "src/carnot/planner/distributed/coordinator/prune_unavailable_sources_rule.h"
#include "src/carnot/planner/ir/memory_source_ir.h"
#include "src/carnot/planner/ir/string_ir.h"
#include "src/carnot/planner/ir/udtf_source_ir.h"
#include "src/carnot/planner/ir/uint128_ir.h"
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
    PX_RETURN_IF_ERROR(DeleteSourceAndChildren(mem_src));
    return true;
  }

  if (!AgentHasTable(mem_src->table_name())) {
    PX_RETURN_IF_ERROR(DeleteSourceAndChildren(mem_src));
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
  PX_RETURN_IF_ERROR(DeleteSourceAndChildren(udtf_src));
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

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
