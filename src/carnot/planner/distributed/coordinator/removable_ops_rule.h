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

#pragma once

#include <algorithm>
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/distributed/coordinator/coordinator.h"
#include "src/carnot/planner/distributed/coordinator/plan_clusters.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief FilterExpressionMatcher is an interface for logic that removes agents that don't match a
 * particular filter (sub-)expression. The front-facing interface consists of MatchExpr which
 * returns whether this FilterExpressionMatcher can be applied and AgentsPruned
 * which returns the set of Agents that do *not* match the expression.
 *
 * TODO(philkuz) figure out how to ensure Match and Create are checked by the parent class.
 * Users can handle new filter expressions by implementing this interface and adding
 * Match and Create static function.
 */
class FilterExpressionMatcher {
 public:
  virtual ~FilterExpressionMatcher() = default;

  /**
   * @brief AgentsPruned returns the set of Agents that do *not* match the filter
   * expression. This outer function contains the logic that uses the the virtual functions
   * ParseExpression and CanAgentRun to handle a particular category of filter
   * expressions. For most implementations, the ParseExpression function should parse the expression
   * for data that will be used in CanAgentRun.
   *
   * @param expr the expression (that should already be matched) to evaluate.
   * @param agents the agents which to check the expression on.
   * @param plan the distributed plan that the agents are a part of.
   * @return StatusOr<AgentSet> the set of agents that do *not* match the filter expression.
   */
  StatusOr<AgentSet> AgentsPruned(ExpressionIR* expr, const absl::flat_hash_set<int64_t>& agents,
                                  DistributedPlan* plan);

 protected:
  /**
   * @brief ParseExpression parses the expression for values that are necessary for comparisons.
   * Implementations should store these extracted values on the object.
   *
   * @param expr The expression to parse.
   */
  virtual void ParseExpression(ExpressionIR* expr) = 0;

  /**
   * @brief CanAgentRun takes a single CarnotInstance then returns true if
   * the CarnotInstance can run the expression evaluated in ParseExpression.
   *
   * @param carnot the data about Carnot running on an agent.
   * @return true if the carnot instance can run the expression.
   * @return false otherwise
   */
  virtual bool CanAgentRun(CarnotInstance* carnot) const = 0;
};

class MatcherFactory {
  using ExprMatcherFn = std::function<bool(ExpressionIR*)>;
  using CreateFn = std::function<std::unique_ptr<FilterExpressionMatcher>()>;

 public:
  /**
   * @brief Adds the new Matcher Function to this factory. The order with which Add() is called
   * determines the order the expressions are evaluated.
   *
   * @tparam Val the matcher type.
   */
  template <typename Val>
  void Add() {
    matchers.push_back(&Val::MatchExpr);
    create.push_back(&Val::Create);
  }

  /**
   * @brief Compares the value to all of the FilterExpressionMatcher objects found in this Factory.
   * If the expression matches any of the matchers, we evaluate the corresponding
   * FilterExpressionMatcher on that expression. Otherwise returns an empty set.
   *
   * The method iterates through the matchers in the order that Add() was called, using only the
   * first matching FilterExpressionMatcher if there is one.
   *
   * @param expr the expression to evaluate.
   * @param agents the agents which to check the expression on.
   * @param plan the distributed plan that the agents are a part of.
   * @return StatusOr<AgentSet> the set of agents that cannot run this expression or an error if one
   * occurs.
   */
  StatusOr<AgentSet> AgentsPrunedByFilterExpression(ExpressionIR* expr,
                                                    const absl::flat_hash_set<int64_t>& agents,
                                                    DistributedPlan* plan) const {
    DCHECK_EQ(matchers.size(), create.size());
    for (const auto& [i, match_fn] : Enumerate(matchers)) {
      if (match_fn(expr)) {
        auto handler = create[i]();
        return handler->AgentsPruned(expr, agents, plan);
      }
    }
    return AgentSet();
  }

  // The matcher functions to evaluate per expression.
  std::vector<ExprMatcherFn> matchers;
  // The functions to create the matcher objects in case an expression matches one.
  std::vector<CreateFn> create;
};

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
                            const SchemaToAgentsMap& schema_map);

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

  MatcherFactory matchers_factory_;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
