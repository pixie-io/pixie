#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/distributed_coordinator.h"
#include "src/carnot/planner/distributed/distributed_splitter.h"
#include "src/carnot/planner/distributed/distributed_stitcher_rules.h"
#include "src/carnot/planner/distributed/grpc_source_conversion.h"
#include "src/carnot/planner/distributed/plan_clusters.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

class MapRemovableOperatorsRule : public Rule {
 public:
  /**
   * @brief Returns a mapping of operators in the query that can be removed from the plans of
   * corresponding agents.
   *
   * Intended to create a set of Operators that can be removed per agent which will then be used to
   * identify unique plans that are duplicated across all agents in a distributed plan.
   *
   * @param plan The distributed plan which describes all the agents in the system.
   * @param pem_instances The Agent IDs from `plan` that we use to build OperatorToAgentSet.
   * @param query The main plan that will derive all other plans. The source of all operators.
   * @return StatusOr<OperatorToAgentSet> the mapping of removable operators to the agents whose
   * plans can remove those operators.
   */
  static StatusOr<OperatorToAgentSet> GetRemovableOperators(
      DistributedPlan* plan, const SchemaToAgentsMap& agent_schema_map,
      const absl::flat_hash_set<int64_t>& pem_instances, IR* query);

 protected:
  MapRemovableOperatorsRule(DistributedPlan* plan,
                            const absl::flat_hash_set<int64_t>& pem_instances,
                            const SchemaToAgentsMap& schema_map)
      : Rule(nullptr, /*use_topo*/ false, /*reverse_topological_execution*/ false),
        plan_(plan),
        pem_instances_(pem_instances),
        schema_map_(schema_map) {}

  StatusOr<bool> Apply(IRNode* node) override;

  /**
   * @brief Returns the set of agents that are removed by this expression.
   *
   * Will return agents if an expression contains a metadata equal expression predicate
   * as well as the composition of such subexpressions inside ofboolean conjunction.
   *
   * @param expr the filter expression to evaluate.
   * @return AgentSet the set of agents that are filtered out by the expression.
   */
  StatusOr<AgentSet> FilterExpressionMayProduceData(ExpressionIR* expr);

  StatusOr<bool> CheckFilter(FilterIR* filter_ir);

  StatusOr<bool> CheckMemorySource(MemorySourceIR* mem_src_ir);

  StatusOr<bool> CheckUDTFSource(UDTFSourceIR* udtf_ir);

  OperatorToAgentSet op_to_agent_set;
  DistributedPlan* plan_;
  const absl::flat_hash_set<int64_t>& pem_instances_;
  const SchemaToAgentsMap& schema_map_;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
