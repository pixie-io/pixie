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

#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {

Status OperatorIR::AddParent(OperatorIR* parent) {
  DCHECK(!IsSource()) << DebugString();
  DCHECK(!graph()->dag().HasEdge(parent->id(), id()))
      << absl::Substitute("Edge between parent op $0(id=$1) and child op $2(id=$3) exists.",
                          parent->type_string(), parent->id(), type_string(), id());

  parents_.push_back(parent);
  return graph()->AddEdge(parent, this);
}

Status OperatorIR::RemoveParent(OperatorIR* parent) {
  DCHECK(graph()->dag().HasEdge(parent->id(), id()))
      << absl::Substitute("Edge between parent op $0(id=$1) and child op $2(id=$3) does not exist.",
                          parent->type_string(), parent->id(), type_string(), id());
  parents_.erase(std::remove(parents_.begin(), parents_.end(), parent), parents_.end());
  return graph()->DeleteEdge(parent->id(), id());
}

Status OperatorIR::ReplaceParent(OperatorIR* old_parent, OperatorIR* new_parent) {
  DCHECK(graph()->dag().HasEdge(old_parent->id(), id()))
      << absl::Substitute("Edge between parent op $0 and child op $1 does not exist.",
                          old_parent->DebugString(), DebugString());
  for (size_t i = 0; i < parents_.size(); ++i) {
    if (parents_[i] == old_parent) {
      parents_[i] = new_parent;
      graph()->dag().ReplaceParentEdge(id(), old_parent->id(), new_parent->id());
      return Status::OK();
    }
  }

  return CreateIRNodeError("Couldn't find specified parent $0 for $1. Found [$2].",
                           old_parent->DebugString(), DebugString(), ParentsDebugString());
}

// Adds a no-op map in front of duplicate parents for multi-parent operators.
// The DAG structure we use doesn't support multiple edges between the same parents, but we
// also want to support use cases like self join and self union.
StatusOr<std::vector<OperatorIR*>> OperatorIR::HandleDuplicateParents(
    const std::vector<OperatorIR*>& parents) {
  absl::flat_hash_set<OperatorIR*> parent_set;
  std::vector<OperatorIR*> new_parents;
  for (OperatorIR* parent : parents) {
    if (!parent_set.contains(parent)) {
      parent_set.insert(parent);
      new_parents.push_back(parent);
      continue;
    }
    PX_ASSIGN_OR_RETURN(MapIR * map,
                        graph()->CreateNode<MapIR>(ast(), parent, ColExpressionVector{},
                                                   /*keep_input_columns*/ true));
    new_parents.push_back(map);
  }
  return new_parents;
}

Status OperatorIR::PruneOutputColumnsTo(const absl::flat_hash_set<std::string>& output_colnames) {
  DCHECK(is_type_resolved());
  for (const auto& kept_colname : output_colnames) {
    DCHECK(resolved_table_type()->HasColumn(kept_colname)) << kept_colname;
  }

  auto output_cols = output_colnames;
  // Ensure that we always output at least one column, unless it's a result sink,
  // in which case it's ok to output 0 columns.
  if (!Match(this, ResultSink())) {
    if (!output_cols.size()) {
      output_cols.insert(resolved_table_type()->ColumnNames()[0]);
    }
  }
  PX_ASSIGN_OR_RETURN(auto required_columns, PruneOutputColumnsToImpl(output_cols));

  auto new_table = TableType::Create();
  for (const auto& [col_name, col_type] : *std::static_pointer_cast<TableType>(resolved_type())) {
    if (required_columns.contains(col_name)) {
      new_table->AddColumn(col_name, col_type->Copy());
    }
  }
  return SetResolvedType(new_table);
}

std::string OperatorIR::ParentsDebugString() {
  return absl::StrJoin(parents(), ",", [](std::string* out, IRNode* in) {
    absl::StrAppend(out, in->DebugString());
  });
}

std::string OperatorIR::ChildrenDebugString() {
  return absl::StrJoin(Children(), ",", [](std::string* out, IRNode* in) {
    absl::StrAppend(out, in->DebugString());
  });
}

Status OperatorIR::CopyFromNode(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PX_RETURN_IF_ERROR(IRNode::CopyFromNode(node, copied_nodes_map));
  return Status::OK();
}

Status OperatorIR::CopyParentsFrom(const OperatorIR* source_op) {
  DCHECK(parents_.empty());
  for (const auto& parent : source_op->parents()) {
    IRNode* new_parent = graph()->Get(parent->id());
    DCHECK(Match(new_parent, Operator()));
    PX_RETURN_IF_ERROR(AddParent(static_cast<OperatorIR*>(new_parent)));
  }
  return Status::OK();
}

std::vector<OperatorIR*> OperatorIR::Children() const {
  std::vector<OperatorIR*> op_children;
  for (int64_t d : graph()->dag().DependenciesOf(id())) {
    auto ir_node = graph()->Get(d);
    if (ir_node->IsOperator()) {
      op_children.push_back(static_cast<OperatorIR*>(ir_node));
    }
  }
  return op_children;
}

bool OperatorIR::NodeMatches(IRNode* node) { return Match(node, Operator()); }

StatusOr<TypePtr> OperatorIR::DefaultResolveType(const std::vector<TypePtr>& parent_types) {
  // Only one parent is supported by default. If your new OperatorIR needs more than one parent, you
  // must define a ResolveType function on it with the same signature as this function (same
  // return type and args, but should be an instance method not static).
  DCHECK_EQ(1U, parent_types.size());
  return parent_types[0]->Copy();
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
