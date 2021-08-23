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

#include <farmhash.h>
#include <queue>

#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {

using table_store::schema::Relation;

Status IR::AddEdge(int64_t from_node, int64_t to_node) {
  dag_.AddEdge(from_node, to_node);
  return Status::OK();
}

Status IR::AddEdge(const IRNode* from_node, const IRNode* to_node) {
  return AddEdge(from_node->id(), to_node->id());
}

bool IR::HasEdge(const IRNode* from_node, const IRNode* to_node) const {
  return HasEdge(from_node->id(), to_node->id());
}

bool IR::HasEdge(int64_t from_node, int64_t to_node) const {
  return dag_.HasEdge(from_node, to_node);
}

Status IR::DeleteEdge(int64_t from_node, int64_t to_node) {
  DCHECK(dag_.HasEdge(from_node, to_node))
      << absl::Substitute("No edge ($0, $1) exists. (from:'$2', to:'$3')", from_node, to_node,
                          HasNode(from_node) ? Get(from_node)->DebugString() : "DoesNotExist",
                          HasNode(to_node) ? Get(to_node)->DebugString() : "DoesNotExist");
  if (!dag_.HasEdge(from_node, to_node)) {
    return error::InvalidArgument("No edge ($0, $1) exists.", from_node, to_node);
  }
  dag_.DeleteEdge(from_node, to_node);
  return Status::OK();
}

Status IR::DeleteEdge(IRNode* from_node, IRNode* to_node) {
  return DeleteEdge(from_node->id(), to_node->id());
}

Status IR::DeleteOrphansInSubtree(int64_t id) {
  auto children = dag_.DependenciesOf(id);
  // If the node has valid parents, then don't delete it.
  auto parents = dag_.ParentsOf(id);
  if (parents.size()) {
    return Status::OK();
  }
  PL_RETURN_IF_ERROR(DeleteNode(id));
  for (auto child_id : children) {
    PL_RETURN_IF_ERROR(DeleteOrphansInSubtree(child_id));
  }
  return Status::OK();
}

Status IR::DeleteSubtree(int64_t id) {
  for (const auto& p : dag_.ParentsOf(id)) {
    dag_.DeleteEdge(p, id);
  }
  return DeleteOrphansInSubtree(id);
}

Status IR::DeleteNode(int64_t node) {
  if (!dag_.HasNode(node)) {
    return error::InvalidArgument("No node $0 exists in graph.", node);
  }
  dag_.DeleteNode(node);
  id_node_map_.erase(node);
  return Status::OK();
}

StatusOr<IRNode*> IR::MakeNodeWithType(IRNodeType node_type, int64_t new_node_id) {
  switch (node_type) {
#undef PL_IR_NODE
#define PL_IR_NODE(NAME)    \
  case IRNodeType::k##NAME: \
    return MakeNode<NAME##IR>(new_node_id);
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PL_IR_NODE

    case IRNodeType::kAny:
    case IRNodeType::number_of_types:
      break;
  }
  return error::Internal("Received unknown IRNode type");
}

std::string IR::DebugString() {
  std::string debug_string = dag().DebugString() + "\n";
  for (auto const& a : id_node_map_) {
    debug_string += a.second->DebugString() + "\n";
  }
  return debug_string;
}

std::string IR::OperatorsDebugString() {
  std::string debug_string;
  for (int64_t i : dag().TopologicalSort()) {
    if (Match(Get(i), Operator())) {
      auto op = static_cast<OperatorIR*>(Get(i));
      std::string parents = absl::StrJoin(op->parents(), ",", [](std::string* out, OperatorIR* in) {
        absl::StrAppend(out, in->id());
      });

      debug_string += absl::Substitute("[$0] $1 \n", parents, op->DebugString());
    }
  }
  return debug_string;
}

std::vector<OperatorIR*> IR::GetSources() const {
  std::vector<OperatorIR*> operators;
  for (auto* node : FindNodesThatMatch(SourceOperator())) {
    operators.push_back(static_cast<OperatorIR*>(node));
  }
  return operators;
}

std::vector<absl::flat_hash_set<int64_t>> IR::IndependentGraphs() const {
  auto sources = FindNodesThatMatch(SourceOperator());
  CHECK(!sources.empty()) << "no source operators found";

  std::unordered_map<int64_t, int64_t> set_parents;
  // The map that keeps track of the actual sets.
  std::unordered_map<int64_t, absl::flat_hash_set<int64_t>> out_map;
  for (auto s : sources) {
    auto source_op = static_cast<OperatorIR*>(s);

    int64_t current_set_parent = s->id();
    set_parents[current_set_parent] = current_set_parent;
    // The queue of children to iterate through.
    std::queue<OperatorIR*> q;
    q.push(source_op);

    absl::flat_hash_set<int64_t> current_set({current_set_parent});
    // Iterate through the children.
    while (!q.empty()) {
      OperatorIR* cur_parent = q.front();
      auto children = cur_parent->Children();
      q.pop();
      for (OperatorIR* child : children) {
        if (set_parents.find(child->id()) != set_parents.end()) {
          // If the child has already been visited, then it already belongs to another set.
          // Point to that set, and merge the existing set.
          int64_t new_set_parent = set_parents[child->id()];
          set_parents[current_set_parent] = new_set_parent;
          current_set_parent = new_set_parent;
          out_map[new_set_parent].merge(current_set);
          current_set = out_map[new_set_parent];

        } else {
          // If the child has been visited, its children have already been visited.
          q.push(child);
        }
        current_set.insert(child->id());
        set_parents[child->id()] = current_set_parent;
      }
    }
    out_map[current_set_parent] = current_set;
  }

  std::vector<absl::flat_hash_set<int64_t>> out_vec;
  for (const auto& [out_map_id, set] : out_map) {
    if (set_parents[out_map_id] != out_map_id) {
      continue;
    }
    out_vec.push_back(set);
  }

  return out_vec;
}

Status IRNode::CopyFromNode(const IRNode* node,
                            absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  line_ = node->line_;
  col_ = node->col_;
  line_col_set_ = node->line_col_set_;
  ast_ = node->ast_;
  resolved_type_ = node->resolved_type_;
  return CopyFromNodeImpl(node, copied_nodes_map);
}

void IRNode::SetLineCol(int64_t line, int64_t col) {
  line_ = line;
  col_ = col;
  line_col_set_ = true;
}
void IRNode::SetLineCol(const pypa::AstPtr& ast) {
  ast_ = ast;
  SetLineCol(ast->line, ast->column);
}

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
    PL_ASSIGN_OR_RETURN(MapIR * map,
                        graph()->CreateNode<MapIR>(ast(), parent, ColExpressionVector{},
                                                   /*keep_input_columns*/ true));
    new_parents.push_back(map);
  }
  return new_parents;
}

Status OperatorIR::PruneOutputColumnsTo(const absl::flat_hash_set<std::string>& output_colnames) {
  DCHECK(IsRelationInit());
  for (const auto& kept_colname : output_colnames) {
    DCHECK(relation_.HasColumn(kept_colname)) << kept_colname;
  }

  auto output_cols = output_colnames;
  // Ensure that we always output at least one column, unless it's a result sink,
  // in which case it's ok to output 0 columns.
  if (!Match(this, ResultSink())) {
    if (!output_cols.size()) {
      output_cols.insert(relation().col_names()[0]);
    }
  }
  PL_ASSIGN_OR_RETURN(auto required_columns, PruneOutputColumnsToImpl(output_cols));
  Relation updated_relation;
  for (const auto& colname : relation_.col_names()) {
    if (required_columns.contains(colname)) {
      updated_relation.AddColumn(relation_.GetColumnType(colname), colname,
                                 relation_.GetColumnSemanticType(colname),
                                 relation_.GetColumnDesc(colname));
    }
  }

  // TODO(james, PP-2065): Once relation is removed we should turn this into a DCHECK.
  if (is_type_resolved()) {
    auto new_table = TableType::Create();
    for (const auto& [col_name, col_type] : *std::static_pointer_cast<TableType>(resolved_type())) {
      if (required_columns.contains(col_name)) {
        new_table->AddColumn(col_name, col_type->Copy());
      }
    }
    PL_RETURN_IF_ERROR(SetResolvedType(new_table));
  }
  return SetRelation(updated_relation);
}

std::string IRNode::DebugString() const {
  return absl::Substitute("$0(id=$1)", type_string(), id());
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

std::string MemorySourceIR::DebugString() const {
  return absl::Substitute("$0(id=$1, table=$2, streaming=$3)", type_string(), id(), table_name_,
                          streaming_);
}

Status MemorySourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_mem_source_op();
  op->set_op_type(planpb::MEMORY_SOURCE_OPERATOR);
  pb->set_name(table_name_);

  if (!column_index_map_set()) {
    return error::InvalidArgument("MemorySource columns are not set.");
  }

  DCHECK_EQ(column_index_map_.size(), relation().NumColumns());
  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->add_column_idxs(column_index_map_[i]);
    pb->add_column_names(relation().col_names()[i]);
    pb->add_column_types(relation().col_types()[i]);
  }

  if (IsTimeSet()) {
    auto start_time = new ::google::protobuf::Int64Value();
    start_time->set_value(time_start_ns_);
    pb->set_allocated_start_time(start_time);
    auto stop_time = new ::google::protobuf::Int64Value();
    stop_time->set_value(time_stop_ns_);
    pb->set_allocated_stop_time(stop_time);
  }

  if (HasTablet()) {
    pb->set_tablet(tablet_value());
  }

  pb->set_streaming(streaming());
  return Status::OK();
}

Status EmptySourceIR::Init(const Relation& relation) {
  PL_RETURN_IF_ERROR(SetRelation(relation));
  return Status::OK();
}
Status EmptySourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_empty_source_op();
  op->set_op_type(planpb::EMPTY_SOURCE_OPERATOR);

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->add_column_names(relation().col_names()[i]);
    pb->add_column_types(relation().col_types()[i]);
  }

  return Status::OK();
}

Status EmptySourceIR::CopyFromNodeImpl(const IRNode* source,
                                       absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const EmptySourceIR* empty = static_cast<const EmptySourceIR*>(source);
  return SetRelation(empty->relation());
}

StatusOr<absl::flat_hash_set<std::string>> EmptySourceIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>&) {
  return error::Unimplemented("Cannot prune columns for empty source.");
}

Status MemorySinkIR::Init(OperatorIR* parent, const std::string& name,
                          const std::vector<std::string> out_columns) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  name_ = name;
  out_columns_ = out_columns;
  return Status::OK();
}

Status MemorySourceIR::Init(const std::string& table_name,
                            const std::vector<std::string>& select_columns) {
  table_name_ = table_name;
  column_names_ = select_columns;
  return Status::OK();
}

Status MemorySourceIR::SetTimeExpressions(ExpressionIR* new_start_time_expr,
                                          ExpressionIR* new_end_time_expr) {
  CHECK(new_start_time_expr != nullptr);
  CHECK(new_end_time_expr != nullptr);

  auto old_start_time_expr = start_time_expr_;
  auto old_end_time_expr = end_time_expr_;

  if (start_time_expr_) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_start_time_expr));
  }
  if (end_time_expr_) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_end_time_expr));
  }

  PL_ASSIGN_OR_RETURN(start_time_expr_,
                      graph()->OptionallyCloneWithEdge(this, new_start_time_expr));
  PL_ASSIGN_OR_RETURN(end_time_expr_, graph()->OptionallyCloneWithEdge(this, new_end_time_expr));

  if (old_start_time_expr) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_start_time_expr->id()));
  }
  if (old_end_time_expr) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_end_time_expr->id()));
  }

  has_time_expressions_ = true;
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> MemorySourceIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_colnames) {
  DCHECK(column_index_map_set());
  DCHECK(IsRelationInit());
  std::vector<std::string> new_col_names;
  std::vector<int64_t> new_col_index_map;

  auto col_names = relation().col_names();
  for (const auto& [idx, name] : Enumerate(col_names)) {
    if (output_colnames.contains(name)) {
      new_col_names.push_back(name);
      new_col_index_map.push_back(column_index_map_[idx]);
    }
  }
  if (new_col_names != relation().col_names()) {
    column_names_ = new_col_names;
  }
  column_index_map_ = new_col_index_map;
  return output_colnames;
}

Status MemorySinkIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_mem_sink_op();
  pb->set_name(name_);
  op->set_op_type(planpb::MEMORY_SINK_OPERATOR);

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  if (is_type_resolved()) {
    auto table_type = std::static_pointer_cast<TableType>(resolved_type());
    for (const auto& col_name : names) {
      if (table_type->HasColumn(col_name)) {
        PL_ASSIGN_OR_RETURN(auto col_type, table_type->GetColumnType(col_name));
        if (!col_type->IsValueType()) {
          return error::Internal("Attempting to create MemorySink with a non-columnar type.");
        }
        auto val_type = std::static_pointer_cast<ValueType>(col_type);
        pb->add_column_semantic_types(val_type->semantic_type());
      }
    }
  }

  return Status::OK();
}

Status MapIR::SetColExprs(const ColExpressionVector& exprs) {
  auto old_exprs = col_exprs_;
  col_exprs_.clear();
  for (const ColumnExpression& expr : old_exprs) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, expr.node));
  }
  for (const auto& expr : exprs) {
    PL_RETURN_IF_ERROR(AddColExpr(expr));
  }
  for (const ColumnExpression& expr : old_exprs) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(expr.node->id()));
  }
  return Status::OK();
}

Status MapIR::AddColExpr(const ColumnExpression& expr) {
  PL_ASSIGN_OR_RETURN(auto expr_node, graph()->OptionallyCloneWithEdge(this, expr.node));
  col_exprs_.emplace_back(expr.name, expr_node);
  return Status::OK();
}

Status MapIR::UpdateColExpr(std::string_view name, ExpressionIR* expr) {
  PL_ASSIGN_OR_RETURN(auto expr_node, graph()->OptionallyCloneWithEdge(this, expr));
  for (size_t i = 0; i < col_exprs_.size(); ++i) {
    if (col_exprs_[i].name == name) {
      auto old_expr = col_exprs_[i].node;
      col_exprs_[i].node = expr_node;
      PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_expr));
      return graph()->DeleteOrphansInSubtree(old_expr->id());
    }
  }
  return error::Internal("Column $0 does not exist in Map", name);
}

Status MapIR::UpdateColExpr(ExpressionIR* old_expr, ExpressionIR* new_expr) {
  PL_ASSIGN_OR_RETURN(auto expr_node, graph()->OptionallyCloneWithEdge(this, new_expr));
  for (size_t i = 0; i < col_exprs_.size(); ++i) {
    if (col_exprs_[i].node == old_expr) {
      col_exprs_[i].node = expr_node;
      PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_expr));
      return graph()->DeleteOrphansInSubtree(old_expr->id());
    }
  }
  return error::Internal("Expression $0 does not exist in Map", old_expr->DebugString());
}

Status MapIR::Init(OperatorIR* parent, const ColExpressionVector& col_exprs,
                   bool keep_input_columns) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  PL_RETURN_IF_ERROR(SetColExprs(col_exprs));
  keep_input_columns_ = keep_input_columns;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> MapIR::RequiredInputColumns() const {
  absl::flat_hash_set<std::string> required;
  for (const auto& expr : col_exprs_) {
    PL_ASSIGN_OR_RETURN(auto inputs, expr.node->InputColumnNames());
    required.insert(inputs.begin(), inputs.end());
  }
  return std::vector<absl::flat_hash_set<std::string>>{required};
}

StatusOr<absl::flat_hash_set<std::string>> MapIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_colnames) {
  ColExpressionVector new_exprs;
  for (const auto& expr : col_exprs_) {
    if (output_colnames.contains(expr.name)) {
      new_exprs.push_back(expr);
    }
  }
  PL_RETURN_IF_ERROR(SetColExprs(new_exprs));
  return output_colnames;
}

Status DropIR::Init(OperatorIR* parent, const std::vector<std::string>& drop_cols) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  col_names_ = drop_cols;
  return Status::OK();
}

Status DropIR::ToProto(planpb::Operator*) const {
  return error::Unimplemented("$0 does not have a protobuf.", type_string());
}

Status MapIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_map_op();
  op->set_op_type(planpb::MAP_OPERATOR);

  for (const auto& col_expr : col_exprs_) {
    auto expr = pb->add_expressions();
    PL_RETURN_IF_ERROR(col_expr.node->ToProto(expr));
    pb->add_column_names(col_expr.name);
  }

  return Status::OK();
}

Status FilterIR::Init(OperatorIR* parent, ExpressionIR* expr) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  return SetFilterExpr(expr);
}

std::string FilterIR::DebugString() const {
  return absl::Substitute("$0(id=$1, expr=$2)", type_string(), id(), filter_expr_->DebugString());
}

Status FilterIR::SetFilterExpr(ExpressionIR* expr) {
  auto old_filter_expr = filter_expr_;
  if (old_filter_expr) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_filter_expr));
  }
  PL_ASSIGN_OR_RETURN(filter_expr_, graph()->OptionallyCloneWithEdge(this, expr));
  if (old_filter_expr) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_filter_expr->id()));
  }
  return Status::OK();
}

absl::flat_hash_set<std::string> ColumnsFromRelation(Relation r) {
  auto col_names = r.col_names();
  return {col_names.begin(), col_names.end()};
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> FilterIR::RequiredInputColumns() const {
  DCHECK(IsRelationInit());
  PL_ASSIGN_OR_RETURN(auto filter_cols, filter_expr_->InputColumnNames());
  auto relation_cols = ColumnsFromRelation(relation());
  filter_cols.insert(relation_cols.begin(), relation_cols.end());
  return std::vector<absl::flat_hash_set<std::string>>{filter_cols};
}

StatusOr<absl::flat_hash_set<std::string>> FilterIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_cols) {
  return output_cols;
}

Status FilterIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_filter_op();
  op->set_op_type(planpb::FILTER_OPERATOR);
  DCHECK_EQ(parents().size(), 1UL);

  auto col_names = relation().col_names();
  for (const auto& col_name : col_names) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parents()[0]->id());
    auto parent_relation = parents()[0]->relation();
    DCHECK(parent_relation.HasColumn(col_name)) << absl::Substitute(
        "Don't have $0 in parent_relation $1", col_name, parent_relation.DebugString());
    col_pb->set_index(parent_relation.GetColumnIndex(col_name));
  }

  auto expr = pb->mutable_expression();
  PL_RETURN_IF_ERROR(filter_expr_->ToProto(expr));
  return Status::OK();
}

Status LimitIR::Init(OperatorIR* parent, int64_t limit_value, bool pem_only) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  SetLimitValue(limit_value);
  pem_only_ = pem_only;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> LimitIR::RequiredInputColumns() const {
  DCHECK(IsRelationInit());
  return std::vector<absl::flat_hash_set<std::string>>{ColumnsFromRelation(relation())};
}

Status LimitIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_limit_op();
  op->set_op_type(planpb::LIMIT_OPERATOR);
  DCHECK_EQ(parents().size(), 1UL);

  auto parent_rel = parents()[0]->relation();
  auto parent_id = parents()[0]->id();

  for (const std::string& col_name : relation().col_names()) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parent_id);
    col_pb->set_index(parent_rel.GetColumnIndex(col_name));
  }
  if (!limit_value_set_) {
    return CreateIRNodeError("Limit value not set properly.");
  }

  pb->set_limit(limit_value_);
  for (const auto src_id : abortable_srcs_) {
    pb->add_abortable_srcs(src_id);
  }
  return Status::OK();
}

Status BlockingAggIR::Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups,
                           const ColExpressionVector& agg_expr) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  PL_RETURN_IF_ERROR(SetGroups(groups));
  return SetAggExprs(agg_expr);
}

Status BlockingAggIR::SetAggExprs(const ColExpressionVector& agg_exprs) {
  auto old_agg_expressions = aggregate_expressions_;
  for (const ColumnExpression& agg_expr : aggregate_expressions_) {
    ExpressionIR* expr = agg_expr.node;
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, expr));
  }
  aggregate_expressions_.clear();

  for (const auto& agg_expr : agg_exprs) {
    PL_ASSIGN_OR_RETURN(auto updated_expr, graph()->OptionallyCloneWithEdge(this, agg_expr.node));
    aggregate_expressions_.emplace_back(agg_expr.name, updated_expr);
  }

  for (const auto& old_agg_expr : old_agg_expressions) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_agg_expr.node->id()));
  }

  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> BlockingAggIR::RequiredInputColumns()
    const {
  absl::flat_hash_set<std::string> required;
  for (const auto& group : groups()) {
    required.insert(group->col_name());
  }
  for (const auto& agg_expr : aggregate_expressions_) {
    PL_ASSIGN_OR_RETURN(auto ret, agg_expr.node->InputColumnNames());
    required.insert(ret.begin(), ret.end());
  }
  return std::vector<absl::flat_hash_set<std::string>>{required};
}

StatusOr<absl::flat_hash_set<std::string>> BlockingAggIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_colnames) {
  absl::flat_hash_set<std::string> kept_columns = output_colnames;

  ColExpressionVector new_aggs;
  for (const auto& expr : aggregate_expressions_) {
    if (output_colnames.contains(expr.name)) {
      new_aggs.push_back(expr);
    }
  }
  PL_RETURN_IF_ERROR(SetAggExprs(new_aggs));

  // We always need to keep the group columns based on the current specification of aggregate,
  // otherwise the result will change.
  for (const ColumnIR* group : groups()) {
    kept_columns.insert(group->col_name());
  }
  return kept_columns;
}

Status GroupByIR::Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  return SetGroups(groups);
}

Status GroupByIR::SetGroups(const std::vector<ColumnIR*>& groups) {
  DCHECK(groups_.empty());
  groups_.resize(groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    PL_ASSIGN_OR_RETURN(groups_[i], graph()->OptionallyCloneWithEdge(this, groups[i]));
  }
  return Status::OK();
}

Status GroupByIR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const GroupByIR* group_by = static_cast<const GroupByIR*>(source);
  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : group_by->groups_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_column, graph()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }
  return SetGroups(new_groups);
}

Status BlockingAggIR::EvaluateAggregateExpression(planpb::AggregateExpression* expr,
                                                  const ExpressionIR& ir_node) const {
  DCHECK(ir_node.type() == IRNodeType::kFunc);
  auto casted_ir = static_cast<const FuncIR&>(ir_node);
  expr->set_name(casted_ir.func_name());
  expr->set_id(casted_ir.func_id());
  for (types::DataType dt : casted_ir.registry_arg_types()) {
    expr->add_args_data_types(dt);
  }
  for (auto ir_arg : casted_ir.args()) {
    auto arg_pb = expr->add_args();
    if (ir_arg->IsColumn()) {
      PL_RETURN_IF_ERROR(static_cast<ColumnIR*>(ir_arg)->ToProto(arg_pb->mutable_column()));
    } else if (ir_arg->IsData()) {
      PL_RETURN_IF_ERROR(static_cast<DataIR*>(ir_arg)->ToProto(arg_pb->mutable_constant()));
    } else {
      return CreateIRNodeError("$0 is an invalid aggregate value", ir_arg->type_string());
    }
  }
  return Status::OK();
}

Status BlockingAggIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_agg_op();
  if (finalize_results_ && !partial_agg_) {
    (*pb->mutable_values()) = pre_split_proto_.values();
    (*pb->mutable_value_names()) = pre_split_proto_.value_names();
  } else {
    for (const auto& agg_expr : aggregate_expressions_) {
      auto expr = pb->add_values();
      PL_RETURN_IF_ERROR(EvaluateAggregateExpression(expr, *agg_expr.node));
      pb->add_value_names(agg_expr.name);
    }
  }
  for (ColumnIR* group : groups()) {
    auto group_pb = pb->add_groups();
    PL_RETURN_IF_ERROR(group->ToProto(group_pb));
    pb->add_group_names(group->col_name());
  }

  pb->set_windowed(false);
  pb->set_partial_agg(partial_agg_);
  pb->set_finalize_results(finalize_results_);

  op->set_op_type(planpb::AGGREGATE_OPERATOR);
  return Status::OK();
}

StatusOr<absl::flat_hash_set<ColumnIR*>> ExpressionIR::InputColumns() {
  if (Match(this, DataNode())) {
    return absl::flat_hash_set<ColumnIR*>{};
  }
  if (Match(this, ColumnNode())) {
    return absl::flat_hash_set<ColumnIR*>{static_cast<ColumnIR*>(this)};
  }
  if (!Match(this, Func())) {
    return error::Internal("Unexpected Expression type: $0", DebugString());
  }
  absl::flat_hash_set<ColumnIR*> ret;
  auto func = static_cast<FuncIR*>(this);
  for (ExpressionIR* arg : func->args()) {
    PL_ASSIGN_OR_RETURN(auto input_cols, arg->InputColumns());
    ret.insert(input_cols.begin(), input_cols.end());
  }
  return ret;
}

StatusOr<absl::flat_hash_set<std::string>> ExpressionIR::InputColumnNames() {
  PL_ASSIGN_OR_RETURN(auto cols, InputColumns());
  absl::flat_hash_set<std::string> output;
  for (auto col : cols) {
    output.insert(col->col_name());
  }
  return output;
}

Status DataIR::ToProto(planpb::ScalarExpression* expr) const {
  auto value_pb = expr->mutable_constant();
  return ToProto(value_pb);
}

Status DataIR::ToProto(planpb::ScalarValue* value_pb) const {
  value_pb->set_data_type(evaluated_data_type_);
  return ToProtoImpl(value_pb);
}

uint64_t DataIR::HashValue() const { return HashValueImpl(); }

StatusOr<DataIR*> DataIR::FromProto(IR* ir, std::string_view name,
                                    const planpb::ScalarValue& value) {
  switch (value.data_type()) {
    case types::BOOLEAN: {
      return ir->CreateNode<BoolIR>(/*ast*/ nullptr, value.bool_value());
    }
    case types::INT64: {
      return ir->CreateNode<IntIR>(/*ast*/ nullptr, value.int64_value());
    }
    case types::FLOAT64: {
      return ir->CreateNode<FloatIR>(/*ast*/ nullptr, value.float64_value());
    }
    case types::STRING: {
      return ir->CreateNode<StringIR>(/*ast*/ nullptr, value.string_value());
    }
    case types::UINT128: {
      std::string upid_str =
          sole::rebuild(value.uint128_value().high(), value.uint128_value().low()).str();
      return ir->CreateNode<UInt128IR>(/*ast*/ nullptr, upid_str);
    }
    case types::TIME64NS: {
      return ir->CreateNode<TimeIR>(/*ast*/ nullptr, value.time64_ns_value());
    }
    default: {
      return error::InvalidArgument("Error processing $0: $1 not handled as a default data type.",
                                    name, types::ToString(value.data_type()));
    }
  }
}

StatusOr<DataIR*> DataIR::ZeroValueForType(IR* ir, types::DataType type) {
  PL_ASSIGN_OR_RETURN(auto ir_type, DataTypeToIRNodeType(type));
  return ZeroValueForType(ir, ir_type);
}

StatusOr<DataIR*> DataIR::ZeroValueForType(IR* ir, IRNodeType type) {
  switch (type) {
    case IRNodeType::kBool:
      return ir->CreateNode<BoolIR>(/*ast*/ nullptr, false);
    case IRNodeType::kFloat:
      return ir->CreateNode<FloatIR>(/*ast*/ nullptr, 0);
    case IRNodeType::kInt:
      return ir->CreateNode<IntIR>(/*ast*/ nullptr, 0);
    case IRNodeType::kString:
      return ir->CreateNode<StringIR>(/*ast*/ nullptr, "");
    case IRNodeType::kTime:
      return ir->CreateNode<TimeIR>(/*ast*/ nullptr, 0);
    case IRNodeType::kUInt128:
      return ir->CreateNode<UInt128IR>(/*ast*/ nullptr, 0);
    default:
      CHECK(false) << absl::Substitute("Invalid IRNodeType for DataIR: $0", TypeString(type));
  }
}

Status IntIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_int64_value(val_);
  return Status::OK();
}

uint64_t IntIR::HashValueImpl() const {
  return ::util::Hash64(reinterpret_cast<const char*>(&val_), sizeof(int64_t));
}

Status FloatIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_float64_value(val_);
  return Status::OK();
}

uint64_t FloatIR::HashValueImpl() const {
  return ::util::Hash64(reinterpret_cast<const char*>(&val_), sizeof(double));
}

Status BoolIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_bool_value(val_);
  return Status::OK();
}

uint64_t BoolIR::HashValueImpl() const {
  return ::util::Hash64(reinterpret_cast<const char*>(&val_), sizeof(bool));
}

Status StringIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_string_value(str_);
  return Status::OK();
}

uint64_t StringIR::HashValueImpl() const { return ::util::Hash64(str_); }

Status TimeIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_time64_ns_value(val_);
  return Status::OK();
}

uint64_t TimeIR::HashValueImpl() const {
  return ::util::Hash64(reinterpret_cast<const char*>(&val_), sizeof(int64_t));
}

Status UInt128IR::ToProtoImpl(planpb::ScalarValue* value) const {
  auto uint128pb = value->mutable_uint128_value();
  uint128pb->set_high(absl::Uint128High64(val_));
  uint128pb->set_low(absl::Uint128Low64(val_));
  return Status::OK();
}

uint64_t UInt128IR::HashValueImpl() const {
  uint64_t high_low[] = {absl::Uint128High64(val_), absl::Uint128Low64(val_)};
  return ::util::Hash64(reinterpret_cast<const char*>(high_low), 2 * sizeof(uint64_t));
}

Status ColumnIR::Init(const std::string& col_name, int64_t parent_idx) {
  SetColumnName(col_name);
  SetContainingOperatorParentIdx(parent_idx);
  return Status::OK();
}

StatusOr<int64_t> ColumnIR::GetColumnIndex() const {
  PL_ASSIGN_OR_RETURN(auto op, ReferencedOperator());
  if (!op->relation().HasColumn(col_name())) {
    return DExitOrIRNodeError("Column '$0' does not exist in relation $1", col_name(),
                              op->relation().DebugString());
  }
  return op->relation().GetColumnIndex(col_name());
}

Status ColumnIR::ToProto(planpb::Column* column_pb) const {
  PL_ASSIGN_OR_RETURN(int64_t ref_op_id, ReferenceID());
  column_pb->set_node(ref_op_id);
  PL_ASSIGN_OR_RETURN(auto index, GetColumnIndex());
  column_pb->set_index(index);
  return Status::OK();
}

Status ColumnIR::ToProto(planpb::ScalarExpression* expr) const {
  auto column_pb = expr->mutable_column();
  return ToProto(column_pb);
}

std::string ColumnIR::DebugString() const {
  return absl::Substitute("$0(id=$1, name=$2)", type_string(), id(), col_name());
}
std::string MetadataIR::DebugString() const {
  return absl::Substitute("$0(id=$1, name=$2)", type_string(), id(), name());
}
std::string StringIR::DebugString() const {
  return absl::Substitute("$0(id=$1, val=$2)", type_string(), id(), str());
}

void ColumnIR::SetContainingOperatorParentIdx(int64_t container_op_parent_idx) {
  DCHECK_GE(container_op_parent_idx, 0);
  container_op_parent_idx_ = container_op_parent_idx;
  container_op_parent_idx_set_ = true;
}

StatusOr<std::vector<OperatorIR*>> ColumnIR::ContainingOperators() const {
  std::vector<OperatorIR*> parents;
  std::queue<int64_t> cur_ids;
  cur_ids.push(id());

  while (cur_ids.size()) {
    auto cur_id = cur_ids.front();
    cur_ids.pop();
    IRNode* cur_node = graph()->Get(cur_id);
    if (cur_node->IsOperator()) {
      parents.push_back(static_cast<OperatorIR*>(cur_node));
      continue;
    }
    std::vector<int64_t> parents = graph()->dag().ParentsOf(cur_id);
    for (auto parent_id : parents) {
      cur_ids.push(parent_id);
    }
  }
  return parents;
}

StatusOr<OperatorIR*> ColumnIR::ReferencedOperator() const {
  DCHECK(container_op_parent_idx_set_);
  PL_ASSIGN_OR_RETURN(std::vector<OperatorIR*> containing_ops, ContainingOperators());
  if (!containing_ops.size()) {
    return CreateIRNodeError(
        "Got no containing operators for $0 when looking up referenced operator.", DebugString());
  }
  // While the column may be contained by multiple operators, it must always originate from the same
  // dataframe.
  OperatorIR* referenced = containing_ops[0]->parents()[container_op_parent_idx_];
  for (OperatorIR* containing_op : containing_ops) {
    DCHECK_EQ(referenced, containing_op->parents()[container_op_parent_idx_]);
  }
  return referenced;
}

Status StringIR::Init(std::string str) {
  str_ = str;
  return Status::OK();
}

Status UInt128IR::Init(absl::uint128 val) {
  val_ = val;
  return Status::OK();
}

Status UInt128IR::Init(const std::string& uuid_str) {
  auto upid_or_s = md::UPID::ParseFromUUIDString(uuid_str);
  if (!upid_or_s.ok()) {
    return CreateIRNodeError(upid_or_s.msg());
  }

  return Init(upid_or_s.ConsumeValueOrDie().value());
}

Status UInt128IR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const UInt128IR* input = static_cast<const UInt128IR*>(source);
  val_ = input->val_;
  return Status::OK();
}

std::unordered_map<std::string, FuncIR::Op> FuncIR::op_map{
    {"*", {FuncIR::Opcode::mult, "*", "multiply"}},
    {"+", {FuncIR::Opcode::add, "+", "add"}},
    {"%", {FuncIR::Opcode::mod, "%", "modulo"}},
    {"-", {FuncIR::Opcode::sub, "-", "subtract"}},
    {"/", {FuncIR::Opcode::div, "/", "divide"}},
    {">", {FuncIR::Opcode::gt, ">", "greaterThan"}},
    {"<", {FuncIR::Opcode::lt, "<", "lessThan"}},
    {"==", {FuncIR::Opcode::eq, "==", "equal"}},
    {"!=", {FuncIR::Opcode::neq, "!=", "notEqual"}},
    {"<=", {FuncIR::Opcode::lteq, "<=", "lessThanEqual"}},
    {">=", {FuncIR::Opcode::gteq, ">=", "greaterThanEqual"}},
    {"and", {FuncIR::Opcode::logand, "and", "logicalAnd"}},
    {"or", {FuncIR::Opcode::logor, "or", "logicalOr"}}};

std::unordered_map<std::string, FuncIR::Op> FuncIR::unary_op_map{
    // + as a unary operator returns its operand unchanged.
    {"+", {FuncIR::Opcode::non_op, "+", ""}},
    {"-", {FuncIR::Opcode::negate, "-", "negate"}},
    {"not", {FuncIR::Opcode::lognot, "not", "logicalNot"}},
    {"~", {FuncIR::Opcode::invert, "~", "invert"}},
};

Status FuncIR::Init(Op op, const std::vector<ExpressionIR*>& args) {
  op_ = op;
  for (auto a : args) {
    PL_RETURN_IF_ERROR(AddOrCloneArg(a));
  }
  return Status::OK();
}

Status FuncIR::AddArg(ExpressionIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Argument for FuncIR is null.");
  }
  all_args_.push_back(arg);
  if (is_init_args_split_) {
    args_.push_back(arg);
  }
  return graph()->AddEdge(this, arg);
}

Status FuncIR::AddInitArg(DataIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Init argument for FuncIR is null.");
  }
  init_args_.push_back(arg);
  return graph()->AddEdge(this, arg);
}

Status FuncIR::UpdateArg(int64_t idx, ExpressionIR* arg) {
  CHECK_LT(idx, static_cast<int64_t>(all_args_.size()))
      << "Tried to update arg of index greater than number of args.";
  ExpressionIR* old_arg = all_args_[idx];
  all_args_[idx] = arg;
  if (is_init_args_split_) {
    if (idx < static_cast<int64_t>(init_args_.size())) {
      if (!Match(arg, DataNode())) {
        return CreateIRNodeError(
            "expected init argument $0 to be a primitive data type, received $1", idx,
            arg->type_string());
      }
      init_args_[idx] = static_cast<DataIR*>(arg);
    } else {
      args_[idx] = arg;
    }
  }
  PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_arg));
  PL_RETURN_IF_ERROR(graph()->OptionallyCloneWithEdge(this, arg));
  PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_arg->id()));
  return Status::OK();
}

Status FuncIR::UpdateArg(ExpressionIR* old_arg, ExpressionIR* new_arg) {
  for (size_t i = 0; i < all_args_.size(); ++i) {
    if (all_args_[i] != old_arg) {
      continue;
    }
    return UpdateArg(i, new_arg);
  }
  return error::Internal("Arg $0 does not exist in $1", old_arg->DebugString(), DebugString());
}

Status FuncIR::AddOrCloneArg(ExpressionIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Argument for FuncIR is null.");
  }
  PL_ASSIGN_OR_RETURN(auto updated_arg, graph()->OptionallyCloneWithEdge(this, arg));
  all_args_.push_back(updated_arg);
  return Status::OK();
}

Status FuncIR::ToProto(planpb::ScalarExpression* expr) const {
  auto func_pb = expr->mutable_func();
  func_pb->set_name(func_name());
  func_pb->set_id(func_id_);
  for (DataIR* init_arg : init_args()) {
    PL_RETURN_IF_ERROR(init_arg->ToProto(func_pb->add_init_args()));
  }
  if (!HasRegistryArgTypes()) {
    return CreateIRNodeError("arg types not resolved");
  }
  for (const auto& [idx, arg] : Enumerate(args())) {
    PL_RETURN_IF_ERROR(arg->ToProto(func_pb->add_args()));
    func_pb->add_args_data_types(registry_arg_types_[idx + init_args().size()]);
  }
  return Status::OK();
}

Status FuncIR::SplitInitArgs(size_t num_init_args) {
  if (all_args_.size() < num_init_args) {
    return CreateIRNodeError("expected $0 init args but only received $1 arguments", num_init_args,
                             all_args_.size());
  }
  for (const auto& [idx, arg] : Enumerate(all_args_)) {
    if (idx < num_init_args) {
      if (!Match(arg, DataNode())) {
        return CreateIRNodeError(
            "expected init argument $0 to be a primitive data type, received $1", idx,
            arg->type_string());
      }
      init_args_.push_back(static_cast<DataIR*>(arg));
    } else {
      args_.push_back(arg);
    }
  }
  is_init_args_split_ = true;
  return Status::OK();
}

/* Float IR */
Status FloatIR::Init(double val) {
  val_ = val;
  return Status::OK();
}
/* Int IR */
Status IntIR::Init(int64_t val) {
  val_ = val;
  return Status::OK();
}
/* Bool IR */
Status BoolIR::Init(bool val) {
  val_ = val;
  return Status::OK();
}

/* Time IR */
Status TimeIR::Init(int64_t val) {
  val_ = val;
  return Status::OK();
}

/* Metadata IR */
Status MetadataIR::Init(const std::string& metadata_str, int64_t parent_op_idx) {
  // Note, metadata_str is a temporary name. It is updated in ResolveMetadataColumn.
  PL_RETURN_IF_ERROR(ColumnIR::Init(metadata_str, parent_op_idx));
  metadata_name_ = metadata_str;
  return Status::OK();
}

Status OperatorIR::CopyFromNode(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PL_RETURN_IF_ERROR(IRNode::CopyFromNode(node, copied_nodes_map));
  const OperatorIR* source = static_cast<const OperatorIR*>(node);

  relation_ = source->relation_;
  relation_init_ = source->relation_init_;
  return Status::OK();
}

Status ExpressionIR::CopyFromNode(const IRNode* node,
                                  absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PL_RETURN_IF_ERROR(IRNode::CopyFromNode(node, copied_nodes_map));
  annotations_ = static_cast<const ExpressionIR*>(node)->annotations();
  type_cast_ = static_cast<const ExpressionIR*>(node)->type_cast();
  return Status::OK();
}

// Clone Functions
StatusOr<std::unique_ptr<IR>> IR::Clone() const {
  auto new_ir = std::make_unique<IR>();
  absl::flat_hash_set<int64_t> nodes{dag().nodes().begin(), dag().nodes().end()};
  PL_RETURN_IF_ERROR(new_ir->CopySelectedNodesAndDeps(this, nodes));
  // TODO(philkuz) check to make sure these are the same.
  new_ir->dag_ = dag_;
  return new_ir;
}

Status IR::CopySelectedNodesAndDeps(const IR* src,
                                    const absl::flat_hash_set<int64_t>& selected_nodes) {
  absl::flat_hash_map<const IRNode*, IRNode*> copied_nodes_map;
  // Need to perform the copies in topological sort order to ensure the edges can be successfully
  // added.
  for (int64_t i : src->dag().TopologicalSort()) {
    if (!selected_nodes.contains(i)) {
      continue;
    }
    IRNode* node = src->Get(i);
    if (HasNode(i) && Get(i)->type() == node->type()) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(IRNode * new_node, CopyNode(node, &copied_nodes_map));
    if (new_node->IsOperator()) {
      PL_RETURN_IF_ERROR(static_cast<OperatorIR*>(new_node)->CopyParentsFrom(
          static_cast<const OperatorIR*>(node)));
    }
  }
  return Status::OK();
}

Status IR::CopyOperatorSubgraph(const IR* src,
                                const absl::flat_hash_set<OperatorIR*>& selected_ops) {
  absl::flat_hash_set<int64_t> op_ids;
  for (auto op : selected_ops) {
    op_ids.insert(op->id());
  }
  return CopySelectedNodesAndDeps(src, op_ids);
}

Status OperatorIR::CopyParentsFrom(const OperatorIR* source_op) {
  DCHECK(parents_.empty());
  for (const auto& parent : source_op->parents()) {
    IRNode* new_parent = graph()->Get(parent->id());
    DCHECK(Match(new_parent, Operator()));
    PL_RETURN_IF_ERROR(AddParent(static_cast<OperatorIR*>(new_parent)));
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

Status ColumnIR::CopyFromNode(const IRNode* source,
                              absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PL_RETURN_IF_ERROR(ExpressionIR::CopyFromNode(source, copied_nodes_map));
  const ColumnIR* column = static_cast<const ColumnIR*>(source);
  col_name_ = column->col_name_;
  col_name_set_ = column->col_name_set_;
  evaluated_data_type_ = column->evaluated_data_type_;
  is_data_type_evaluated_ = column->is_data_type_evaluated_;
  container_op_parent_idx_ = column->container_op_parent_idx_;
  container_op_parent_idx_set_ = column->container_op_parent_idx_set_;
  return Status::OK();
}

Status ColumnIR::CopyFromNodeImpl(const IRNode*, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  return Status::OK();
}

Status StringIR::CopyFromNodeImpl(const IRNode* source,
                                  absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const StringIR* input = static_cast<const StringIR*>(source);
  str_ = input->str_;
  return Status::OK();
}

Status FuncIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const FuncIR* func = static_cast<const FuncIR*>(node);
  func_prefix_ = func->func_prefix_;
  op_ = func->op_;
  func_name_ = func->func_name_;
  registry_arg_types_ = func->registry_arg_types_;
  func_id_ = func->func_id_;
  evaluated_data_type_ = func->evaluated_data_type_;
  is_data_type_evaluated_ = func->is_data_type_evaluated_;
  supports_partial_ = func->supports_partial_;
  is_init_args_split_ = func->is_init_args_split_;

  for (const DataIR* init_arg : func->init_args_) {
    PL_ASSIGN_OR_RETURN(DataIR * new_arg, graph()->CopyNode(init_arg, copied_nodes_map));
    PL_RETURN_IF_ERROR(AddInitArg(new_arg));
  }

  for (const ExpressionIR* arg : func->args_) {
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_arg, graph()->CopyNode(arg, copied_nodes_map));
    PL_RETURN_IF_ERROR(AddArg(new_arg));
  }

  // This branch is hit only when the above two for loops looped over nothing.
  if (!is_init_args_split_) {
    for (const ExpressionIR* arg : func->all_args_) {
      PL_ASSIGN_OR_RETURN(ExpressionIR * new_arg, graph()->CopyNode(arg, copied_nodes_map));
      all_args_.push_back(new_arg);
      PL_RETURN_IF_ERROR(graph()->AddEdge(this, new_arg));
    }
  }
  return Status::OK();
}

Status FloatIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const FloatIR* float_ir = static_cast<const FloatIR*>(node);
  val_ = float_ir->val_;
  return Status::OK();
}

Status IntIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const IntIR* int_ir = static_cast<const IntIR*>(node);
  val_ = int_ir->val_;
  return Status::OK();
}

Status BoolIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const BoolIR* bool_ir = static_cast<const BoolIR*>(node);
  val_ = bool_ir->val_;
  return Status::OK();
}

Status TimeIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const TimeIR* time_ir = static_cast<const TimeIR*>(node);
  val_ = time_ir->val_;
  return Status::OK();
}

Status MetadataIR::CopyFromNodeImpl(const IRNode* node,
                                    absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const MetadataIR* metadata_ir = static_cast<const MetadataIR*>(node);
  metadata_name_ = metadata_ir->metadata_name_;
  return Status::OK();
}

Status MemorySourceIR::CopyFromNodeImpl(
    const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const MemorySourceIR* source_ir = static_cast<const MemorySourceIR*>(node);

  table_name_ = source_ir->table_name_;
  time_set_ = source_ir->time_set_;
  time_start_ns_ = source_ir->time_start_ns_;
  time_stop_ns_ = source_ir->time_stop_ns_;
  column_names_ = source_ir->column_names_;
  column_index_map_set_ = source_ir->column_index_map_set_;
  column_index_map_ = source_ir->column_index_map_;
  has_time_expressions_ = source_ir->has_time_expressions_;
  streaming_ = source_ir->streaming_;

  if (has_time_expressions_) {
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_start_expr,
                        graph()->CopyNode(source_ir->start_time_expr_, copied_nodes_map));
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_stop_expr,
                        graph()->CopyNode(source_ir->end_time_expr_, copied_nodes_map));
    PL_RETURN_IF_ERROR(SetTimeExpressions(new_start_expr, new_stop_expr));
  }
  return Status::OK();
}

Status MemorySinkIR::CopyFromNodeImpl(const IRNode* node,
                                      absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const MemorySinkIR* sink_ir = static_cast<const MemorySinkIR*>(node);
  name_ = sink_ir->name_;
  out_columns_ = sink_ir->out_columns_;
  return Status::OK();
}

Status MapIR::CopyFromNodeImpl(const IRNode* node,
                               absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const MapIR* map_ir = static_cast<const MapIR*>(node);
  ColExpressionVector new_col_exprs;
  for (const ColumnExpression& col_expr : map_ir->col_exprs_) {
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                        graph()->CopyNode(col_expr.node, copied_nodes_map));
    new_col_exprs.push_back({col_expr.name, new_node});
  }
  keep_input_columns_ = map_ir->keep_input_columns_;
  return SetColExprs(new_col_exprs);
}

Status DropIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const DropIR* drop = static_cast<const DropIR*>(node);
  col_names_ = drop->col_names_;
  return Status::OK();
}

Status BlockingAggIR::CopyFromNodeImpl(
    const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const BlockingAggIR* blocking_agg = static_cast<const BlockingAggIR*>(node);

  ColExpressionVector new_agg_exprs;
  for (const ColumnExpression& col_expr : blocking_agg->aggregate_expressions_) {
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                        graph()->CopyNode(col_expr.node, copied_nodes_map));
    new_agg_exprs.push_back({col_expr.name, new_node});
  }

  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : blocking_agg->groups()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_column, graph()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }

  PL_RETURN_IF_ERROR(SetAggExprs(new_agg_exprs));
  PL_RETURN_IF_ERROR(SetGroups(new_groups));

  finalize_results_ = blocking_agg->finalize_results_;
  partial_agg_ = blocking_agg->partial_agg_;
  pre_split_proto_ = blocking_agg->pre_split_proto_;

  return Status::OK();
}

Status FilterIR::CopyFromNodeImpl(const IRNode* node,
                                  absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const FilterIR* filter = static_cast<const FilterIR*>(node);
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                      graph()->CopyNode(filter->filter_expr_, copied_nodes_map));
  PL_RETURN_IF_ERROR(SetFilterExpr(new_node));
  return Status::OK();
}

Status LimitIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const LimitIR* limit = static_cast<const LimitIR*>(node);
  limit_value_ = limit->limit_value_;
  limit_value_set_ = limit->limit_value_set_;
  pem_only_ = limit->pem_only_;
  return Status::OK();
}

Status GRPCSinkIR::CopyFromNodeImpl(const IRNode* node,
                                    absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const GRPCSinkIR* grpc_sink = static_cast<const GRPCSinkIR*>(node);
  sink_type_ = grpc_sink->sink_type_;
  destination_id_ = grpc_sink->destination_id_;
  destination_address_ = grpc_sink->destination_address_;
  destination_ssl_targetname_ = grpc_sink->destination_ssl_targetname_;
  name_ = grpc_sink->name_;
  out_columns_ = grpc_sink->out_columns_;
  return Status::OK();
}

Status GRPCSourceGroupIR::CopyFromNodeImpl(const IRNode* node,
                                           absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const GRPCSourceGroupIR* grpc_source_group = static_cast<const GRPCSourceGroupIR*>(node);
  source_id_ = grpc_source_group->source_id_;
  grpc_address_ = grpc_source_group->grpc_address_;
  if (grpc_source_group->dependent_sinks_.size()) {
    return error::Unimplemented("Cannot clone GRPCSourceGroupIR with dependent_sinks_");
  }
  return Status::OK();
}

Status GRPCSourceIR::CopyFromNodeImpl(const IRNode*, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  return Status::OK();
}

Status UnionIR::CopyFromNodeImpl(const IRNode* node,
                                 absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const UnionIR* union_ir = static_cast<const UnionIR*>(node);
  std::vector<InputColumnMapping> dest_mappings;
  for (const InputColumnMapping& src_mapping : union_ir->column_mappings_) {
    InputColumnMapping dest_mapping;
    for (ColumnIR* src_col : src_mapping) {
      PL_ASSIGN_OR_RETURN(ColumnIR * dest_col, graph()->CopyNode(src_col, copied_nodes_map));
      dest_mapping.push_back(dest_col);
    }
    dest_mappings.push_back(dest_mapping);
  }
  return SetColumnMappings(dest_mappings);
}

Status JoinIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const JoinIR* join_node = static_cast<const JoinIR*>(node);
  join_type_ = join_node->join_type_;

  std::vector<ColumnIR*> new_output_columns;
  for (const ColumnIR* col : join_node->output_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_output_columns.push_back(new_node);
  }
  PL_RETURN_IF_ERROR(SetOutputColumns(join_node->column_names_, new_output_columns));

  std::vector<ColumnIR*> new_left_columns;
  for (const ColumnIR* col : join_node->left_on_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_left_columns.push_back(new_node);
  }

  std::vector<ColumnIR*> new_right_columns;
  for (const ColumnIR* col : join_node->right_on_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph()->CopyNode(col, copied_nodes_map));
    new_right_columns.push_back(new_node);
  }

  PL_RETURN_IF_ERROR(SetJoinColumns(new_left_columns, new_right_columns));
  suffix_strs_ = join_node->suffix_strs_;
  return Status::OK();
}

Status GRPCSourceGroupIR::ToProto(planpb::Operator* op) const {
  // Note this is more for testing.
  auto pb = op->mutable_grpc_source_op();
  op->set_op_type(planpb::GRPC_SOURCE_OPERATOR);
  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  return Status::OK();
}

Status GRPCSinkIR::ToProto(planpb::Operator* op) const {
  CHECK(has_output_table());
  auto pb = op->mutable_grpc_sink_op();
  op->set_op_type(planpb::GRPC_SINK_OPERATOR);
  pb->mutable_output_table()->set_table_name(name());
  pb->set_address(destination_address());
  pb->mutable_connection_options()->set_ssl_targetname(destination_ssl_targetname());

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->mutable_output_table()->add_column_types(types[i]);
    pb->mutable_output_table()->add_column_names(names[i]);
  }

  if (is_type_resolved()) {
    auto table_type = std::static_pointer_cast<TableType>(resolved_type());
    for (const auto& col_name : names) {
      if (table_type->HasColumn(col_name)) {
        PL_ASSIGN_OR_RETURN(auto col_type, table_type->GetColumnType(col_name));
        if (!col_type->IsValueType()) {
          return error::Internal("Attempting to create GRPCSink with a non-columnar type.");
        }
        auto val_type = std::static_pointer_cast<ValueType>(col_type);
        pb->mutable_output_table()->add_column_semantic_types(val_type->semantic_type());
      }
    }
  }
  return Status::OK();
}

Status GRPCSinkIR::ToProto(planpb::Operator* op, int64_t agent_id) const {
  auto pb = op->mutable_grpc_sink_op();
  op->set_op_type(planpb::GRPC_SINK_OPERATOR);
  pb->set_address(destination_address());
  pb->mutable_connection_options()->set_ssl_targetname(destination_ssl_targetname());
  if (!agent_id_to_destination_id_.contains(agent_id)) {
    return CreateIRNodeError("No agent ID '$0' found in grpc sink '$1'", agent_id, DebugString());
  }
  pb->set_grpc_source_id(agent_id_to_destination_id_.find(agent_id)->second);
  return Status::OK();
}

Status GRPCSourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_grpc_source_op();
  op->set_op_type(planpb::GRPC_SOURCE_OPERATOR);
  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }
  return Status::OK();
}

StatusOr<planpb::Plan> IR::ToProto() const { return ToProto(0); }

IRNode* IR::Get(int64_t id) const {
  auto iterator = id_node_map_.find(id);
  if (iterator == id_node_map_.end()) {
    return nullptr;
  }
  return iterator->second.get();
}

StatusOr<planpb::Plan> IR::ToProto(int64_t agent_id) const {
  auto plan = planpb::Plan();

  auto plan_dag = plan.mutable_dag();
  auto plan_dag_node = plan_dag->add_nodes();
  plan_dag_node->set_id(1);

  auto plan_fragment = plan.add_nodes();
  plan_fragment->set_id(1);

  absl::flat_hash_set<int64_t> non_op_nodes;
  auto operators = dag().TopologicalSort();
  for (const auto& node_id : operators) {
    auto node = Get(node_id);
    if (node->IsOperator()) {
      PL_RETURN_IF_ERROR(OutputProto(plan_fragment, static_cast<OperatorIR*>(node), agent_id));
    } else {
      non_op_nodes.emplace(node_id);
    }
  }
  dag_.ToProto(plan_fragment->mutable_dag(), non_op_nodes);
  return plan;
}

Status IR::OutputProto(planpb::PlanFragment* pf, const OperatorIR* op_node,
                       int64_t agent_id) const {
  // Check to make sure that the relation is set for this op_node, otherwise it's not connected to
  // a Sink.
  CHECK(op_node->IsRelationInit())
      << absl::Substitute("$0 doesn't have a relation.", op_node->DebugString());

  // Add PlanNode.
  auto plan_node = pf->add_nodes();
  plan_node->set_id(op_node->id());
  auto op_pb = plan_node->mutable_op();
  // Special case for GRPCSinks.
  if (Match(op_node, GRPCSink())) {
    const GRPCSinkIR* grpc_sink = static_cast<const GRPCSinkIR*>(op_node);
    if (grpc_sink->has_output_table()) {
      return grpc_sink->ToProto(op_pb);
    }
    return grpc_sink->ToProto(op_pb, agent_id);
  }

  return op_node->ToProto(op_pb);
}

Status IR::Prune(const absl::flat_hash_set<int64_t>& ids_to_prune) {
  for (auto node : ids_to_prune) {
    for (auto child : dag_.DependenciesOf(node)) {
      PL_RETURN_IF_ERROR(DeleteEdge(node, child));
    }
    for (auto parent : dag_.ParentsOf(node)) {
      PL_RETURN_IF_ERROR(DeleteEdge(parent, node));
    }
    PL_RETURN_IF_ERROR(DeleteNode(node));
  }
  return Status::OK();
}

Status IR::Keep(const absl::flat_hash_set<int64_t>& ids_to_keep) {
  absl::flat_hash_set<int64_t> ids_to_prune;
  for (int64_t n : dag().nodes()) {
    if (ids_to_keep.contains(n)) {
      continue;
    }
    ids_to_prune.insert(n);
  }
  return Prune(ids_to_prune);
}

std::vector<IRNode*> IR::FindNodesOfType(IRNodeType type) const {
  std::vector<IRNode*> nodes;
  for (int64_t i : dag().nodes()) {
    IRNode* node = Get(i);
    if (node->type() == type) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

Status GRPCSourceGroupIR::AddGRPCSink(GRPCSinkIR* sink_op,
                                      const absl::flat_hash_set<int64_t>& agents) {
  if (sink_op->destination_id() != source_id_) {
    return DExitOrIRNodeError("Source id $0 and destination id $1 aren't equal.",
                              sink_op->destination_id(), source_id_);
  }
  if (!GRPCAddressSet()) {
    return DExitOrIRNodeError("$0 doesn't have a physical agent associated with it.",
                              DebugString());
  }
  sink_op->SetDestinationAddress(grpc_address_);
  sink_op->SetDestinationSSLTargetName(ssl_targetname_);
  dependent_sinks_.emplace_back(sink_op, agents);
  return Status::OK();
}

Status UnionIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_union_op();
  op->set_op_type(planpb::UNION_OPERATOR);

  auto types = relation().col_types();
  auto names = relation().col_names();
  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_names(names[i]);
  }
  if (default_column_mapping_) {
    for (size_t parent_i = 0; parent_i < parents().size(); ++parent_i) {
      auto* pb_column_mapping = pb->add_column_mappings();
      for (size_t col_i = 0; col_i < relation().NumColumns(); ++col_i) {
        pb_column_mapping->add_column_indexes(col_i);
      }
    }
    return Status::OK();
  }
  DCHECK_EQ(parents().size(), column_mappings_.size()) << "parents and column_mappings disagree.";
  DCHECK(HasColumnMappings());

  for (const auto& column_mapping : column_mappings_) {
    auto* pb_column_mapping = pb->add_column_mappings();
    for (const ColumnIR* col : column_mapping) {
      PL_ASSIGN_OR_RETURN(auto index, col->GetColumnIndex());
      pb_column_mapping->add_column_indexes(index);
    }
  }

  // NOTE: not setting value as this is set in the execution engine. Keeping this here in case it
  // needs to be modified in the future.
  // pb->set_rows_per_batch(1024);

  return Status::OK();
}

Status UnionIR::AddColumnMapping(const InputColumnMapping& column_mapping) {
  InputColumnMapping cloned_mapping;
  for (ColumnIR* col : column_mapping) {
    PL_ASSIGN_OR_RETURN(auto cloned, graph()->OptionallyCloneWithEdge(this, col));
    cloned_mapping.push_back(cloned);
  }

  column_mappings_.push_back(cloned_mapping);
  return Status::OK();
}

Status UnionIR::SetColumnMappings(const std::vector<InputColumnMapping>& column_mappings) {
  auto old_mappings = column_mappings_;
  column_mappings_.clear();
  for (const auto& old_mapping : old_mappings) {
    for (ColumnIR* col : old_mapping) {
      PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, col));
    }
  }
  for (const auto& new_mapping : column_mappings) {
    PL_RETURN_IF_ERROR(AddColumnMapping(new_mapping));
  }
  for (const auto& old_mapping : old_mappings) {
    for (ColumnIR* col : old_mapping) {
      PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(col->id()));
    }
  }
  return Status::OK();
}

Status UnionIR::SetRelationFromParents() {
  DCHECK(!default_column_mapping_)
      << "Default column mapping set on using the SetRelationFromParents call.";
  DCHECK(!parents().empty());

  std::vector<Relation> relations;
  OperatorIR* base_parent = parents()[0];
  Relation base_relation = base_parent->relation();
  PL_RETURN_IF_ERROR(SetRelation(base_relation));

  std::vector<InputColumnMapping> mappings;
  for (const auto& [parent_idx, parent] : Enumerate(parents())) {
    const Relation& cur_relation = parent->relation();
    if (cur_relation.NumColumns() != base_relation.NumColumns()) {
      return CreateIRNodeError(
          "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
          base_parent->DebugString(), parent->DebugString(), DebugString(),
          base_relation.DebugString(), cur_relation.DebugString(), "Column count wrong.");
    }
    InputColumnMapping column_mapping;
    for (const std::string& col_name : base_relation.col_names()) {
      types::DataType col_type = base_relation.GetColumnType(col_name);
      if (!cur_relation.HasColumn(col_name) || cur_relation.GetColumnType(col_name) != col_type) {
        return CreateIRNodeError(
            "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
            base_parent->DebugString(), parent->DebugString(), DebugString(),
            base_relation.DebugString(), cur_relation.DebugString(),
            absl::Substitute("Missing or wrong type for $0.", col_name));
      }
      PL_ASSIGN_OR_RETURN(auto col_node, graph()->CreateNode<ColumnIR>(
                                             ast(), col_name, /*parent_op_idx*/ parent_idx));
      column_mapping.push_back(col_node);
    }
    mappings.push_back(column_mapping);
  }
  return SetColumnMappings(mappings);
}

Status UnionIR::SetDefaultColumnMapping() {
  if (!IsRelationInit()) {
    return CreateIRNodeError("Relation is not initialized yet");
  }
  DCHECK(!HasColumnMappings())
      << "Trying to set default column mapping on a Union that has a default column mapping.";
  default_column_mapping_ = true;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> UnionIR::RequiredInputColumns() const {
  DCHECK(HasColumnMappings());
  std::vector<absl::flat_hash_set<std::string>> ret;
  for (const auto& column_mapping : column_mappings_) {
    absl::flat_hash_set<std::string> parent_names;
    for (const ColumnIR* input_col : column_mapping) {
      parent_names.insert(input_col->col_name());
    }
    ret.push_back(parent_names);
  }
  return ret;
}

Status UnionIR::Init(const std::vector<OperatorIR*>& parents) {
  // Support joining a table against itself by calling HandleDuplicateParents.
  PL_ASSIGN_OR_RETURN(auto transformed_parents, HandleDuplicateParents(parents));
  for (auto p : transformed_parents) {
    PL_RETURN_IF_ERROR(AddParent(p));
  }
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> UnionIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& kept_columns) {
  std::vector<InputColumnMapping> new_column_mappings;
  for (const auto& parent_column_mapping : column_mappings_) {
    InputColumnMapping new_parent_mapping;
    for (ColumnIR* parent_col : parent_column_mapping) {
      // Union matches parent columns with each other by name, but they may be in different orders
      // in the parents.
      if (kept_columns.contains(parent_col->col_name())) {
        new_parent_mapping.push_back(parent_col);
      }
    }
    new_column_mappings.push_back(new_parent_mapping);
  }
  PL_RETURN_IF_ERROR(SetColumnMappings(new_column_mappings));
  return kept_columns;
}

StatusOr<JoinIR::JoinType> JoinIR::GetJoinEnum(const std::string& join_type_str) const {
  // TODO(philkuz) (PL-1136) convert to enum library friendly version.
  absl::flat_hash_map<std::string, JoinType> join_key_mapping = {{"inner", JoinType::kInner},
                                                                 {"left", JoinType::kLeft},
                                                                 {"outer", JoinType::kOuter},
                                                                 {"right", JoinType::kRight}};
  auto iter = join_key_mapping.find(join_type_str);

  // If the join type is not found, then return an error.
  if (iter == join_key_mapping.end()) {
    std::vector<std::string> valid_join_keys;
    for (auto kv : join_key_mapping) {
      valid_join_keys.push_back(kv.first);
    }
    return CreateIRNodeError("'$0' join type not supported. Only {$1} are available.",
                             join_type_str, absl::StrJoin(valid_join_keys, ","));
  }
  return iter->second;
}

planpb::JoinOperator::JoinType JoinIR::GetPbJoinEnum(JoinType join_type) {
  absl::flat_hash_map<JoinType, planpb::JoinOperator::JoinType> join_key_mapping = {
      {JoinType::kInner, planpb::JoinOperator_JoinType_INNER},
      {JoinType::kLeft, planpb::JoinOperator_JoinType_LEFT_OUTER},
      {JoinType::kOuter, planpb::JoinOperator_JoinType_FULL_OUTER}};
  auto join_key_iter = join_key_mapping.find(join_type);
  CHECK(join_key_iter != join_key_mapping.end()) << "Received an unexpected enum value.";
  return join_key_iter->second;
}

Status JoinIR::ToProto(planpb::Operator* op) const {
  planpb::JoinOperator::JoinType join_enum_type = GetPbJoinEnum(join_type_);
  DCHECK_EQ(left_on_columns_.size(), right_on_columns_.size());
  auto pb = op->mutable_join_op();
  op->set_op_type(planpb::JOIN_OPERATOR);
  pb->set_type(join_enum_type);
  for (int64_t i = 0; i < static_cast<int64_t>(left_on_columns_.size()); i++) {
    auto eq_condition = pb->add_equality_conditions();
    PL_ASSIGN_OR_RETURN(auto left_index, left_on_columns_[i]->GetColumnIndex());
    PL_ASSIGN_OR_RETURN(auto right_index, right_on_columns_[i]->GetColumnIndex());
    eq_condition->set_left_column_index(left_index);
    eq_condition->set_right_column_index(right_index);
  }

  for (ColumnIR* col : output_columns_) {
    auto* parent_col = pb->add_output_columns();
    int64_t parent_idx = col->container_op_parent_idx();
    DCHECK_LT(parent_idx, 2);
    parent_col->set_parent_index(col->container_op_parent_idx());
    DCHECK(col->IsDataTypeEvaluated()) << "Column not evaluated";
    PL_ASSIGN_OR_RETURN(auto index, col->GetColumnIndex());
    parent_col->set_column_index(index);
  }

  for (const auto& col_name : column_names_) {
    *(pb->add_column_names()) = col_name;
  }
  // NOTE: not setting value as this is set in the execution engine. Keeping this here in case it
  // needs to be modified in the future.
  // pb->set_rows_per_batch(1024);

  return Status::OK();
}

Status JoinIR::Init(const std::vector<OperatorIR*>& parents, const std::string& how_type,
                    const std::vector<ColumnIR*>& left_on_cols,
                    const std::vector<ColumnIR*>& right_on_cols,
                    const std::vector<std::string>& suffix_strs) {
  if (left_on_cols.size() != right_on_cols.size()) {
    return CreateIRNodeError("'left_on' and 'right_on' must contain the same number of elements.");
  }

  // Support joining a table against itself by calling HandleDuplicateParents.
  PL_ASSIGN_OR_RETURN(auto transformed_parents, HandleDuplicateParents(parents));
  for (auto* p : transformed_parents) {
    PL_RETURN_IF_ERROR(AddParent(p));
  }

  PL_RETURN_IF_ERROR(SetJoinColumns(left_on_cols, right_on_cols));

  suffix_strs_ = suffix_strs;
  return SetJoinType(how_type);
}

Status JoinIR::SetJoinColumns(const std::vector<ColumnIR*>& left_columns,
                              const std::vector<ColumnIR*>& right_columns) {
  DCHECK(left_on_columns_.empty());
  DCHECK(right_on_columns_.empty());
  left_on_columns_.resize(left_columns.size());
  for (size_t i = 0; i < left_columns.size(); ++i) {
    PL_ASSIGN_OR_RETURN(left_on_columns_[i],
                        graph()->OptionallyCloneWithEdge(this, left_columns[i]));
  }
  right_on_columns_.resize(right_columns.size());
  for (size_t i = 0; i < right_columns.size(); ++i) {
    PL_ASSIGN_OR_RETURN(right_on_columns_[i],
                        graph()->OptionallyCloneWithEdge(this, right_columns[i]));
  }
  key_columns_set_ = true;
  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> JoinIR::RequiredInputColumns() const {
  DCHECK(key_columns_set_);
  DCHECK(!output_columns_.empty());

  std::vector<absl::flat_hash_set<std::string>> ret(2);

  for (ColumnIR* col : output_columns_) {
    DCHECK(col->container_op_parent_idx_set());
    ret[col->container_op_parent_idx()].insert(col->col_name());
  }
  for (ColumnIR* col : left_on_columns_) {
    DCHECK(col->container_op_parent_idx_set());
    ret[col->container_op_parent_idx()].insert(col->col_name());
  }
  for (ColumnIR* col : right_on_columns_) {
    DCHECK(col->container_op_parent_idx_set());
    ret[col->container_op_parent_idx()].insert(col->col_name());
  }

  return ret;
}

Status JoinIR::SetOutputColumns(const std::vector<std::string>& column_names,
                                const std::vector<ColumnIR*>& columns) {
  DCHECK_EQ(column_names.size(), columns.size());
  auto old_output_cols = output_columns_;

  output_columns_ = columns;
  column_names_ = column_names;

  for (auto old_col : old_output_cols) {
    PL_RETURN_IF_ERROR(graph()->DeleteEdge(this, old_col));
  }
  for (auto new_col : output_columns_) {
    PL_RETURN_IF_ERROR(graph()->AddEdge(this, new_col));
  }
  for (auto old_col : old_output_cols) {
    PL_RETURN_IF_ERROR(graph()->DeleteOrphansInSubtree(old_col->id()));
  }
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> JoinIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& kept_columns) {
  DCHECK(!column_names_.empty());
  std::vector<ColumnIR*> new_output_cols;
  std::vector<std::string> new_output_names;
  for (const auto& [col_idx, col_name] : Enumerate(column_names_)) {
    if (kept_columns.contains(col_name)) {
      new_output_names.push_back(col_name);
      new_output_cols.push_back(output_columns_[col_idx]);
    }
  }
  PL_RETURN_IF_ERROR(SetOutputColumns(new_output_names, new_output_cols));
  return kept_columns;
}

Status UDTFSourceIR::Init(std::string_view func_name,
                          const absl::flat_hash_map<std::string, ExpressionIR*>& arg_value_map,
                          const udfspb::UDTFSourceSpec& udtf_spec) {
  func_name_ = func_name;
  udtf_spec_ = udtf_spec;
  table_store::schema::Relation relation;
  PL_RETURN_IF_ERROR(relation.FromProto(&udtf_spec_.relation()));
  PL_RETURN_IF_ERROR(SetRelation(relation));
  return InitArgValues(arg_value_map, udtf_spec);
}

Status UDTFSourceIR::ToProto(planpb::Operator* op) const {
  op->set_op_type(planpb::UDTF_SOURCE_OPERATOR);

  auto pb = op->mutable_udtf_source_op();
  pb->set_name(func_name_);

  for (const auto& arg_value : arg_values_) {
    PL_RETURN_IF_ERROR(arg_value->ToProto(pb->add_arg_values()));
  }

  return Status::OK();
}

Status UDTFSourceIR::SetArgValues(const std::vector<ExpressionIR*>& arg_values) {
  arg_values_.resize(arg_values.size());
  for (const auto& [idx, value] : Enumerate(arg_values)) {
    if (!value->IsData()) {
      return CreateIRNodeError("expected scalar value, received '$0'", value->type_string());
    }
    PL_ASSIGN_OR_RETURN(arg_values_[idx],
                        graph()->OptionallyCloneWithEdge(this, static_cast<DataIR*>(value)));
  }
  return Status::OK();
}

Status UDTFSourceIR::InitArgValues(
    const absl::flat_hash_map<std::string, ExpressionIR*>& arg_value_map,
    const udfspb::UDTFSourceSpec& udtf_spec) {
  std::vector<ExpressionIR*> arg_values;
  arg_values.reserve(udtf_spec.args().size());

  // This gets the arguments in the order of the UDTF spec.
  for (const auto& arg_spec : udtf_spec.args()) {
    auto arg_value_map_iter = arg_value_map.find(arg_spec.name());
    DCHECK(arg_value_map_iter != arg_value_map.end());
    ExpressionIR* arg = arg_value_map_iter->second;
    arg_values.push_back(arg);
  }
  return SetArgValues(arg_values);
}

Status UDTFSourceIR::CopyFromNodeImpl(
    const IRNode* source, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const UDTFSourceIR* udtf = static_cast<const UDTFSourceIR*>(source);
  func_name_ = udtf->func_name_;
  udtf_spec_ = udtf->udtf_spec_;
  std::vector<ExpressionIR*> arg_values;
  for (const DataIR* arg_element : udtf->arg_values_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_arg_element, graph()->CopyNode(arg_element, copied_nodes_map));
    DCHECK(Match(new_arg_element, DataNode()));
    arg_values.push_back(static_cast<DataIR*>(new_arg_element));
  }
  return SetArgValues(arg_values);
}

bool OperatorIR::NodeMatches(IRNode* node) { return Match(node, Operator()); }
bool StringIR::NodeMatches(IRNode* node) { return Match(node, String()); }
bool IntIR::NodeMatches(IRNode* node) { return Match(node, Int()); }
bool FuncIR::NodeMatches(IRNode* node) { return Match(node, Func()); }
bool ExpressionIR::NodeMatches(IRNode* node) { return Match(node, Expression()); }

Status RollingIR::Init(OperatorIR* parent, ColumnIR* window_col, ExpressionIR* window_size) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  PL_RETURN_IF_ERROR(SetWindowCol(window_col));
  PL_RETURN_IF_ERROR(SetWindowSize(window_size));
  return Status::OK();
}

Status RollingIR::SetWindowSize(ExpressionIR* window_size) {
  PL_ASSIGN_OR_RETURN(window_size_, graph()->OptionallyCloneWithEdge(this, window_size));
  return Status::OK();
}

Status RollingIR::ReplaceWindowSize(ExpressionIR* new_window_size) {
  if (new_window_size->id() == window_size_->id()) {
    return Status::OK();
  }
  if (window_size_ == nullptr) {
    return SetWindowSize(new_window_size);
  }
  PL_RETURN_IF_ERROR(graph()->DeleteNode(window_size_->id()));
  PL_RETURN_IF_ERROR(SetWindowSize(new_window_size));
  return Status::OK();
}

Status RollingIR::SetWindowCol(ColumnIR* window_col) {
  PL_ASSIGN_OR_RETURN(window_col_, graph()->OptionallyCloneWithEdge(this, window_col));
  return Status::OK();
}

Status RollingIR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const RollingIR* rolling_node = static_cast<const RollingIR*>(source);
  PL_ASSIGN_OR_RETURN(IRNode * new_window_col,
                      graph()->CopyNode(rolling_node->window_col(), copied_nodes_map));
  DCHECK(Match(new_window_col, ColumnNode()));
  PL_RETURN_IF_ERROR(SetWindowCol(static_cast<ColumnIR*>(new_window_col)));
  PL_ASSIGN_OR_RETURN(IRNode * new_window_size,
                      graph()->CopyNode(rolling_node->window_size(), copied_nodes_map));
  DCHECK(Match(new_window_size, DataNode()));
  PL_RETURN_IF_ERROR(SetWindowSize(static_cast<DataIR*>(new_window_size)));
  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : rolling_node->groups()) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_column, graph()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }
  return SetGroups(new_groups);
}

Status RollingIR::ToProto(planpb::Operator* /* op */) const {
  return CreateIRNodeError("Rolling operator not yet implemented");
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> RollingIR::RequiredInputColumns() const {
  return error::Unimplemented("Rolling operator doesn't support RequiredInputColumns.");
}

StatusOr<absl::flat_hash_set<std::string>> RollingIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& /* output_colnames */) {
  return error::Unimplemented("Rolling operator doesn't support PruneOutputColumntTo.");
}

StatusOr<px::types::DataType> IRNodeTypeToDataType(IRNodeType type) {
  switch (type) {
    case IRNodeType::kString: {
      return px::types::DataType::STRING;
    }
    case IRNodeType::kInt: {
      return px::types::DataType::INT64;
    }
    case IRNodeType::kUInt128: {
      return px::types::DataType::UINT128;
    }
    case IRNodeType::kBool: {
      return px::types::DataType::BOOLEAN;
    }
    case IRNodeType::kFloat: {
      return px::types::DataType::FLOAT64;
    }
    case IRNodeType::kTime: {
      return px::types::DataType::TIME64NS;
    }
    default: {
      return error::InvalidArgument("IRNode type: $0 cannot be converted into literal type.",
                                    magic_enum::enum_name(type));
    }
  }
}

StatusOr<IRNodeType> DataTypeToIRNodeType(types::DataType type) {
  switch (type) {
    case px::types::DataType::STRING: {
      return IRNodeType::kString;
    }
    case px::types::DataType::INT64: {
      return IRNodeType::kInt;
    }
    case px::types::DataType::UINT128: {
      return IRNodeType::kUInt128;
    }
    case px::types::DataType::BOOLEAN: {
      return IRNodeType::kBool;
    }
    case px::types::DataType::FLOAT64: {
      return IRNodeType::kFloat;
    }
    case px::types::DataType::TIME64NS: {
      return IRNodeType::kTime;
    }
    default: {
      return error::InvalidArgument("data type: '$0' cannot be converted into an IRNodeType",
                                    types::ToString(type));
    }
  }
}

Status ResolveOperatorType(OperatorIR* op, CompilerState* compiler_state) {
  if (op->is_type_resolved()) {
    return Status::OK();
  }
  op->PullParentTypes();
  switch (op->type()) {
#undef PL_IR_NODE
#define PL_IR_NODE(NAME)    \
  case IRNodeType::k##NAME: \
    return OperatorTraits<NAME##IR>::ResolveType(static_cast<NAME##IR*>(op), compiler_state);
#include "src/carnot/planner/ir/operators.inl"
#undef PL_IR_NODE
    default:
      return error::Internal(
          absl::Substitute("cannot resolve operator type for non-operator: $0", op->type_string()));
  }
}

Status CheckTypeCast(ExpressionIR* expr, std::shared_ptr<ValueType> from_type,
                     std::shared_ptr<ValueType> to_type) {
  if (from_type->data_type() != to_type->data_type()) {
    return expr->CreateIRNodeError(
        "Cannot cast from '$0' to '$1'. Only semantic type casts are allowed.",
        types::ToString(from_type->data_type()), types::ToString(to_type->data_type()));
  }
  return Status::OK();
}

Status ResolveExpressionType(ExpressionIR* expr, CompilerState* compiler_state,
                             const std::vector<TypePtr>& parent_types) {
  if (expr->is_type_resolved()) {
    return Status::OK();
  }
  Status status;
  switch (expr->type()) {
#undef PL_IR_NODE
#define PL_IR_NODE(NAME)                                                                           \
  case IRNodeType::k##NAME:                                                                        \
    status = ExpressionTraits<NAME##IR>::ResolveType(static_cast<NAME##IR*>(expr), compiler_state, \
                                                     parent_types);                                \
    break;
#include "src/carnot/planner/ir/expressions.inl"
#undef PL_IR_NODE
    default:
      return error::Internal(absl::Substitute(
          "cannot resolve expression type for non-expression: $0", expr->type_string()));
  }
  if (!status.ok() || !expr->HasTypeCast()) {
    return status;
  }
  PL_RETURN_IF_ERROR(CheckTypeCast(expr, std::static_pointer_cast<ValueType>(expr->resolved_type()),
                                   expr->type_cast()));
  return expr->SetResolvedType(expr->type_cast());
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
