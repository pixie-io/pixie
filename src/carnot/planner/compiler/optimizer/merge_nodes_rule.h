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
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

struct MatchingSet {
  // Operators that all are mergable.
  std::vector<OperatorIR*> operators;
};

/**
 * @brief MergeNodesRule searches for duplicate Operator sub-graphs and merges them together.
 *
 * The live-view execution model means a number of operator sub-graphs are duplicated, often
 * entirely duplicated. This also solves many cases of queries that are inefficiently re-using
 * multiple sources, meaning query writers are more free in composing functions together without
 * worrying too much about optimization.
 *
 */
class MergeNodesRule : public Rule {
 public:
  explicit MergeNodesRule(CompilerState* compiler_state)
      : Rule(compiler_state, /*use_topo*/ false, /*reverse_topological_execution*/ false) {}

  /**
   * @brief The entry method that merges nodes within the graph together.
   *
   * The high level algorithm starts by finding matching sets of memory sources.  A matching set is
   * all operators that can be merged into one. We insert the matching sets  into a queue. We read a
   * matching set from that queue, create a single "merged" operator for that matching set and
   * replace operators with the new merged one.
   *
   * The children of all those operators are then passed into the matching sets finder, and each
   * matching set is pushed into the queue.
   *
   * @param graph
   * @return StatusOr<bool> true if the graph is merged, false otherwise. Status contains an error
   * if the graph has some structural problems.
   */
  StatusOr<bool> Execute(IR* graph) override;

  /**
   * @brief CanMerge returns whether two Operators can merge in line with our merging criteria.
   *
   * @param a operator to compare
   * @param b operator to compare
   * @return true: the operators can merge into one.
   * @return false: the operators cannot merge into one.
   */
  bool CanMerge(OperatorIR* a, OperatorIR* b);

  /**
   * @brief Returns each subset of the passed in operators that can be merged together. Each subset
   * returned is mutually exclusive -> there are no overlaps between each set. If all the operators
   * CanMerge, the method returns 1 MatchingSet. If none CanMerge, the method returns len(operators)
   * MatchingSets.
   *
   * For now, each operator is compared to each other operator, meaning this is an O(n^2) operation.
   *
   * @param operators the operators set of which to find merge-able subsets.
   * @return std::vector<MatchingSet> all matching subsets in the operator.
   */
  std::vector<MatchingSet> FindMatchingSets(const std::vector<OperatorIR*>& operators);

  /**
   * @brief Returns a operator that is the combination of all operators in operators_to_merge. An
   * operator returned is new and will never be an element in the operators_to_merge
   * vector.
   *
   * @param graph: the graph to create the Operator in.
   * @param operators_to_merge: the vector of operators_to_merge.
   * @return StatusOr<OperatorIR*>: the merged Operator or an error in the Status if the method runs
   * into an error.
   */
  StatusOr<OperatorIR*> MergeOps(IR* graph, const std::vector<OperatorIR*>& operators_to_merge);

 private:
  // TODO(philkuz) need to remove the dependency on Rule so we don't have to override Apply().
  StatusOr<bool> Apply(IRNode*) override {
    CHECK(false) << "This apply call shouldn't be made.";
    return false;
  }
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
