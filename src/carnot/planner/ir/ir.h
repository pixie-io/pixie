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
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>

#include "src/carnot/dag/dag.h"
#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/ir/ir_node_traits.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class ExpressionIR;
class OperatorIR;

/**
 * IR contains the intermediate representation of the query
 * before compiling into the logical plan.
 */
class IR {
 public:
  /**
   * @brief Node factory that adds a node to the list,
   * updates an id, then returns a pointer to manipulate.
   *
   * The object will be owned by the IR object that created it.
   *
   * @tparam TOperator the type of the operator.
   * @return StatusOr<TOperator *> - the node will be owned
   * by this IR object.
   */
  template <typename TOperator>
  StatusOr<TOperator*> MakeNode(const pypa::AstPtr& ast) {
    return MakeNode<TOperator>(id_node_counter, ast);
  }
  template <typename TOperator>
  StatusOr<TOperator*> MakeNode(int64_t id) {
    return MakeNode<TOperator>(id, nullptr);
  }
  template <typename TOperator>
  StatusOr<TOperator*> MakeNode(int64_t id, const pypa::AstPtr& ast) {
    id_node_counter = std::max(id + 1, id_node_counter);
    auto node = std::make_unique<TOperator>(id);
    dag_.AddNode(node->id());
    node->set_graph(this);
    if (ast != nullptr) {
      node->SetLineCol(ast);
    }
    TOperator* raw = node.get();
    id_node_map_.emplace(node->id(), std::move(node));
    return raw;
  }
  StatusOr<IRNode*> MakeNodeWithType(IRNodeType node_type, int64_t new_node_id);

  template <typename TOperator, typename... Args>
  StatusOr<TOperator*> CreateNode(const pypa::AstPtr& ast, Args&&... args) {
    PX_ASSIGN_OR_RETURN(TOperator * op, MakeNode<TOperator>(ast));
    PX_RETURN_IF_ERROR(op->Init(std::forward<Args>(args)...));
    return op;
  }

  template <typename TIRNodeType>
  StatusOr<TIRNodeType*> CopyNode(const TIRNodeType* source) {
    absl::flat_hash_map<const IRNode*, IRNode*> mapping;
    return CopyNode(source, &mapping);
  }

  /**
   * @brief Copies a node into this IR. Uses the source's node ID if the source is from a different
   * IR. Note that it does not set the parent of the copy, but children of this node will have their
   * parents set.
   *
   * @param source the input node
   * @param copied_nodes_map The mapping of nodes that have already been copied by other calls to
   * CopyNode in the current top-level invocation.
   * @return StatusOr<IRNode*> the copied node
   */
  template <typename TIRNodeType>
  StatusOr<TIRNodeType*> CopyNode(const TIRNodeType* source,
                                  absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
    CHECK(copied_nodes_map != nullptr);
    // If this node has already been copied over, don't copy it again. This will happen when a node
    // has multiple nodes using it.
    if (copied_nodes_map->contains(source)) {
      return static_cast<TIRNodeType*>(copied_nodes_map->at(source));
    }
    // Use the source's ID if we are copying in to a different graph.
    auto new_node_id = this == source->graph() ? id_node_counter : source->id();
    DCHECK(!HasNode(new_node_id)) << source->DebugString();
    PX_ASSIGN_OR_RETURN(IRNode * new_node, MakeNodeWithType(source->type(), new_node_id));
    PX_RETURN_IF_ERROR(new_node->CopyFromNode(source, copied_nodes_map));
    copied_nodes_map->emplace(source, new_node);
    CHECK_EQ(new_node->type(), source->type());
    return static_cast<TIRNodeType*>(new_node);
  }

  Status AddEdge(int64_t from_node, int64_t to_node);
  Status AddEdge(const IRNode* from_node, const IRNode* to_node);

  bool HasEdge(int64_t from_node, int64_t to_node) const;
  bool HasEdge(const IRNode* from_node, const IRNode* to_node) const;

  bool HasNode(int64_t node_id) const { return dag().HasNode(node_id); }

  Status DeleteEdge(int64_t from_node, int64_t to_node);
  Status DeleteEdge(IRNode* from_node, IRNode* to_node);
  Status DeleteNode(int64_t node);
  Status DeleteOrphansInSubtree(int64_t node);

  /**
   * @brief Deletes the subtree of the graph that are orphans after this id is removed.
   *
   * @param id the id of the first node to delete.
   * @return Status: error if any occurs.
   */
  Status DeleteSubtree(int64_t id);

  /**
   * @brief Adds an edge between the parent and child nodes in the DAG.
   * If there is already a link between them, then the child is cloned and an edge is added between
   * the parent and the clone instead.
   *
   * @param parent the parent node
   * @param parent the child node
   * @return StatusOr<IRNode*> the child that now has the edge (either a clone or the input child)
   */
  template <typename TChildType>
  StatusOr<TChildType*> OptionallyCloneWithEdge(IRNode* parent, TChildType* child) {
    TChildType* returned_child = child;
    if (HasEdge(parent, child)) {
      PX_ASSIGN_OR_RETURN(returned_child, CopyNode(child));
    }
    PX_RETURN_IF_ERROR(AddEdge(parent, returned_child));
    return returned_child;
  }

  plan::DAG& dag() { return dag_; }
  const plan::DAG& dag() const { return dag_; }
  std::string DebugString() const;
  std::string OperatorsDebugString();

  /**
   * @brief Returns the corresponding node for the passed in ID. Returns a nullptr when the id
   * doesn't exist.
   *
   * @param id the id of the node to get.
   * @return IRNode* The Node or a nullptr if not found.
   */
  IRNode* Get(int64_t id) const;

  size_t size() const { return id_node_map_.size(); }

  std::vector<OperatorIR*> GetSources() const;

  StatusOr<std::unique_ptr<IR>> Clone() const;

  /**
   * @brief Copies the selected operators from src into the current IR, including their edges
   * and dependencies.
   * @param src: The source IR which contains the nodes to copy.
   * @param selected_nodes: list of operators to copy from src. should be comprised of complete,
   * weakly connected subgraphs from src.
   * Note: Skips node IDs already present in the IR.
   */
  Status CopyOperatorSubgraph(const IR* src, const absl::flat_hash_set<OperatorIR*>& subgraph);

  /**
   * @brief Outputs the proto representation of this plan.
   *
   * @return StatusOr<planpb::Plan>
   */
  StatusOr<planpb::Plan> ToProto() const;
  StatusOr<planpb::Plan> ToProto(int64_t agent_id) const;

  /**
   * @brief Removes the nodes and edges listed in the following set.
   *
   * @param ids_to_prune: the ids which to prune from the graph.
   * @return Status: error if something not found or missing.
   */
  Status Prune(const absl::flat_hash_set<int64_t>& ids_to_prune);

  /**
   * @brief Keeps only the specified nodes.
   *
   * @param ids_to_keep
   * @return Status
   */
  Status Keep(const absl::flat_hash_set<int64_t>& ids_to_keep);

  /**
   * @brief IndependentGraphs returns the sets of OperatorIR ids from this graph that are uniquely
   * connected.
   *
   * @return std::vector<absl::flat_hash_set<int64_t>>
   */
  std::vector<absl::flat_hash_set<int64_t>> IndependentGraphs() const;

  /**
   * @brief Returns nodes that match the IRNodeType.
   *
   * @param type
   * @return std::vector<IRNode*>
   */
  std::vector<IRNode*> FindNodesOfType(IRNodeType type) const;

  /**
   * @brief Returns nodes that match the Matcher.
   *
   * @param type
   * @return std::vector<IRNode*>
   */
  template <typename Matcher>
  std::vector<IRNode*> FindNodesThatMatch(Matcher matcher) const {
    std::vector<IRNode*> nodes;
    for (int64_t i : dag().nodes()) {
      if (Match(Get(i), matcher)) {
        nodes.push_back(Get(i));
      }
    }
    return nodes;
  }

  friend std::ostream& operator<<(std::ostream& os, const std::shared_ptr<IR>&) {
    return os << "ir";
  }

 private:
  Status OutputProto(planpb::PlanFragment* pf, const OperatorIR* op_node, int64_t agent_id) const;
  // Helper function for Clone and CopySelectedOperators.
  Status CopySelectedNodesAndDeps(const IR* src, const absl::flat_hash_set<int64_t>& selected_ids);

  plan::DAG dag_;
  std::unordered_map<int64_t, IRNodePtr> id_node_map_;
  int64_t id_node_counter = 0;
};

Status ResolveOperatorType(OperatorIR* op, CompilerState* compiler_state);

Status ResolveExpressionType(ExpressionIR* expr, CompilerState* compiler_state,
                             const std::vector<TypePtr>& parent_types);

Status PropagateTypeChangesFromNode(IR* graph, IRNode* node, CompilerState* compiler_state);
}  // namespace planner
}  // namespace carnot
}  // namespace px
