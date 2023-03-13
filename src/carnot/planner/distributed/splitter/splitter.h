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
#include <memory>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/splitter/partial_op_mgr/partial_op_mgr.h"
#include "src/carnot/planner/ir/grpc_sink_ir.h"
#include "src/carnot/planner/ir/grpc_source_group_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/rules/rule_executor.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

/**
 * @brief A plan that is split around blocking nodes.
 * before_blocking: plan should have no blocking nodes and should end with nodes that feed into
 * GRPCSinks. No blocking nodes means there also should not be MemorySinks.
 *
 * after_blocking: plan should have no memory sources, feed data in from GRPCSources and sink data
 * into MemorySinks.
 *
 */
struct BlockingSplitPlan {
  // The plan that occcurs before blocking nodes.
  std::unique_ptr<IR> before_blocking;
  // The plan that occcurs after and including blocking nodes.
  std::unique_ptr<IR> after_blocking;
  // The that has both the before and after blocking nodes.
  std::unique_ptr<IR> original_plan;
};

/**
 * @brief Two sets of nodes that correspond to the nodes of the original plan for those
 * that occur before blocking nodes and those that occur after.
 */
struct BlockingSplitNodeIDGroups {
  absl::flat_hash_set<int64_t> before_blocking_nodes;
  absl::flat_hash_set<int64_t> after_blocking_nodes;
};

/**
 * @brief The Splitter splits apart the graph along Blocking Node lines. The result is
 * two new IR graphs -> one that is run on Carnot instances that pull up data from Stirling and the
 * other that is run on Carnot instances which accumulate data and run blocking operations.
 */
class Splitter : public NotCopyable {
 public:
  /**
   * @brief Inserts a GRPCBridge in front of blocking operators in a graph.
   * Inserts a GRPCBridge (GRPCSink -> GRPCSourceGroup) between the parent_op
   * and blocking ops. The returned SplitPlan should contain two IRs now:
   * 1. Where all sources are MemorySources and all sinks are GRPCSinks
   * 2. Where all sources are GRPCSourceGroups and all sinks are MemorySinks
   *
   * Graphically, we want to be able to convert the following logical plan:
   * MemSrc1
   *  |   \
   *  |    Agg
   *  |   /
   * Join
   *  |
   * Sink
   *
   * Into
   * MemSrc1
   *  |
   * GRPCSink(1)
   *
   * GRPCSource(1)
   *  |   \
   *  |    Agg
   *  |   /
   * Join
   *  |
   * Sink
   *
   * Where GRPCSink and GRPCSource are a bridge.
   *
   * @param logical_plan: the input logical_plan
   * @return StatusOr<std::unique_ptr<BlockingSplitPLan>>: the plan split along blocking lines.
   */
  StatusOr<std::unique_ptr<BlockingSplitPlan>> SplitKelvinAndAgents(const IR* logical_plan);

  static StatusOr<std::unique_ptr<Splitter>> Create(CompilerState* compiler_state,
                                                    bool support_partial_agg) {
    std::unique_ptr<Splitter> splitter = std::unique_ptr<Splitter>(new Splitter(compiler_state));
    PX_RETURN_IF_ERROR(splitter->Init(support_partial_agg));
    return splitter;
  }

 private:
  explicit Splitter(CompilerState* compiler_state) : compiler_state_(compiler_state) {}
  Status Init(bool support_partial_agg) {
    if (support_partial_agg) {
      partial_operator_mgrs_.push_back(std::make_unique<AggOperatorMgr>());
    }
    partial_operator_mgrs_.push_back(std::make_unique<LimitOperatorMgr>());
    return Status::OK();
  }
  /**
   * @brief Returns the list of operator ids from the graph that occur before the blocking node and
   * after the blocking node.
   *
   * Note: this does not include non Operator IDs. IR::Keep() with either set of ids
   * will not produce a working graph.
   *
   * @param logical_plan
   * @param on_kelvin
   * @return BlockingSplitNodeIDGroups
   */
  BlockingSplitNodeIDGroups GetSplitGroups(const IR* logical_plan,
                                           const absl::flat_hash_map<int64_t, bool>& on_kelvin);

  StatusOr<absl::flat_hash_map<int64_t, bool>> GetKelvinNodes(
      const IR* logical_plan, const std::vector<int64_t>& source_ids);
  absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>> GetEdgesToBreak(
      const IR* logical_plan, const absl::flat_hash_map<int64_t, bool>& on_kelvin,
      const std::vector<int64_t>& sources);

  StatusOr<std::unique_ptr<IR>> CreateGRPCBridgePlan(
      const IR* logical_plan, const absl::flat_hash_map<int64_t, bool>& on_kelvin,
      const std::vector<int64_t>& sources);
  StatusOr<GRPCSinkIR*> CreateGRPCSink(OperatorIR* parent_op, int64_t grpc_id);
  StatusOr<GRPCSourceGroupIR*> CreateGRPCSourceGroup(OperatorIR* parent_op, int64_t grpc_id);
  Status InsertGRPCBridge(IR* plan, OperatorIR* parent, const std::vector<OperatorIR*>& parents);
  PartialOperatorMgr* GetPartialOperatorMgr(OperatorIR* op) const;
  StatusOr<absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>> ConsolidateEdges(
      const IR* logical_plan,
      const absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>& edges);

  /**
   * The GRPCBridgeTree data structure is used as part of the GRPCBridge consolidation step,
   * organizing GRPCbridges that can be merged together on a shared operator edge instead of
   * existing as independent GRPC Bridges.
   *
   * We might choose to merge the GRPCBridges in a GRPCBridgeTree if the heuristic network cost of
   * the individual GRPCBridges exceeds the cost of a single merged GRPCBridge.
   *
   * A slight caveat to this model is some Operators must be run on PEM, meaning they cannot exist
   * after a GRPCBridge. Best practice is to make those nodes their own GRPCBridgeTrees and marking
   * `must_be_on_pem` as true. Processor should make sure not to merge those Children GRPCBridgeTree
   * nodes that fit this property.
   *
   * For a better understanding of these concepts, the tests in distributed_splitter have
   * accompanying documentation and diagrams for the resulting split graphs.
   */
  struct GRPCBridgeTree {
    // The operator where this bridge node was added. This will be the operator that we will start
    // at if we decided to push-down the GRPCBridges in this node.
    OperatorIR* starting_op;
    // The starting operator of the bridges that belong to this GRPCBridgeTree. Acts as a key in the
    // original GRPCBridge map.
    std::vector<OperatorIR*> bridges;
    // Children GRPCBridgeTrees are descendents of `starting_op` that are not necessarily bridges
    // but can be considered rough equivalents of a bridge, with the added caveat that children
    // GRPCBridgeTrees can be subdivided into its own GRPCBridges (and GRPCBridgeTree children),
    // applying the same heuristic calculations as done on the parent tree.
    std::vector<GRPCBridgeTree> children;

    /**
     * @brief Add a Child to this GRPCBridgeTree.
     *
     * @param child
     */
    void AddChild(const GRPCBridgeTree& child) { children.push_back(child); }

    /**
     * @brief DebugString that makes it easier to understand what's going on in the tree.
     *
     * @return std::string
     */
    std::string DebugString() const {
      std::vector<std::string> children_debug_strings;
      children_debug_strings.push_back(absl::Substitute("$0", starting_op->DebugString()));
      children_debug_strings.push_back(absl::Substitute(
          "\tbridges:$0", absl::StrJoin(bridges, ",", [](std::string* out, const OperatorIR* op) {
            out->append(op->DebugString());
          })));
      for (const auto& c : children) {
        children_debug_strings.push_back(
            "\t" + absl::StrJoin(absl::StrSplit(c.DebugString(), "\n"), "\n\t"));
      }
      return absl::StrJoin(children_debug_strings, "\n");
    }

    // Whether all the bridges have a partial mgr. If true and HasChildrenThatMustBeOnPem() is
    // false, then we heuristically determine that `has_children_that_must_be_on_pem` is false then
    // we don't need to consolidate the GRPC Bridges into a single one. Otherwise,
    bool all_bridges_partial_mgr = true;
    // Whether this BridgeTree starting_op must be on PEM.
    bool must_be_on_pem = false;
    // Whether any `children` must be on pem.
    bool HasChildrenThatMustBeOnPem() const {
      for (const auto& c : children) {
        if (c.HasChildrenThatMustBeOnPem() || c.must_be_on_pem) {
          return true;
        }
      }
      return false;
    }
  };

  /**
   * @brief ConstructGRPCBridgeTree creates an internal graph rooted on the GRPCBridgeTree
   containing
   * all of the GRPCBridges that belong to the "starting_op" as well as any children
   * GRPCBridgeTrees. Children GRPCBridgeTrees typically start at non-bridge OperatorIRs that would
   * be efficient as separate GRPCBridges even if their children are not necessarily efficient.
   *
   * For now, we support operators includes sparse Filters that can be pruned during the
   * coordination stage, such as  `df.ctx['service'] == 'pl/vizier-metadata'`, but can  be extended
   * to other filters, expensive Maps, etc.
   *

   * @param op The current operator and it's children to look at.
   * @param node The node representing the current set of GRPCBridges that can be consolidated
   (pushed-down)
   * @param grpc_bridges The original GRPC Bridges to consolidate.
   */
  void ConstructGRPCBridgeTree(
      OperatorIR* op, GRPCBridgeTree* node,
      const absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>& grpc_bridges);

  /**
   * @brief ExtractBridgesFromGRPCBridgeTree traverses the GRPCBridgeTree tree and outputs the new
   * grpc bridges as such: A GRPCBridgeTree can be either transformed into a single, pushed down
   * GRPCBridge or a set of a GRPCBridges depending on the current heuristic decision function. For
   * now, we use the heuristic that among a set of bridges that come from the same source are only
   * more efficient than a single bridge close to the source if each bridge in the set applies to a
   * partially evaluated blocking operation. If that heuristic is met, we copy the GRPCBridges owned
   * by this node into the output and recurse over children GRPCBridgeTrees with the same
   * evaluation.
   *
   * @param node The node representing the current set of GRPCBridges that can be consolidated
   * (pushed-down)
   * @param new_grpc_bridges The new, consolidated bridges.
   * @param old_grpc_bridges The original GRPC bridges to consolidate.
   */
  Status ExtractBridgesFromGRPCBridgeTree(
      const GRPCBridgeTree& blob,
      absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>* new_grpc_bridges,
      const absl::flat_hash_map<OperatorIR*, std::vector<OperatorIR*>>& old_grpc_bridges);

  /**
   * @brief Returns true if we should start a new node in the GRPCBridgeTree
   * constructor with this operator. Any operator that comes up true for this should be more
   * optimally run on a PEM (closer to the data collection) because it reduces the overall cost of a
   * query execution.
   *
   * This currently only includes a check for Filters that can eliminate certain agents from this
   * part of the plan, but nothing is preventing future other operators such as drop heavy maps from
   * being included here.
   *
   * If amending this in the future, consider that your change has second order effects that can
   * cause queries to become less performant because ExtractBridgesFromGRPCBridgeTree() will
   * consider any matched operators as a viable target to put before a GRPCBridge, leading to higher
   * network utilization for a query.
   *
   * @param operator
   * @return true
   * @return false
   */
  bool CanBeGRPCBridgeTree(OperatorIR* op);

  /**
   * @brief Returns true if each child has a partial operator version. If each child does, then we
   * can convert them to their partial aggregate forms which will reduce the data sent over the
   * network.
   *
   * If it's the case that any of the children lack a partial implementation, that means the parent
   * operator has to send over all of it's data. For the children that do have a partial
   * implementation that means we now not only send over the original full data but we also send
   * over the partial results so our network usage has acutally increased rather than decreased.
   *
   * Instead we opt to just perform the original operation after the GRPCBridge, assuming that the
   * operation is much cheaper than the network costs.
   *
   * @param children
   * @return true
   * @return false
   */
  bool AllHavePartialMgr(std::vector<OperatorIR*> children) const;

  int64_t grpc_id_counter_ = 0;
  std::vector<std::unique_ptr<PartialOperatorMgr>> partial_operator_mgrs_;
  CompilerState* compiler_state_ = nullptr;
};

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
