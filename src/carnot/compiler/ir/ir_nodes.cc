#include <queue>

#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace compiler {

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
      << absl::Substitute("No edge ($0, $1) exists.", from_node, to_node);
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
#include "src/carnot/compiler/ir/ir_nodes.inl"
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

std::vector<OperatorIR*> IR::GetSources() const {
  std::vector<OperatorIR*> operators;
  for (int64_t i : dag().TopologicalSort()) {
    if (Match(Get(i), SourceOperator())) {
      operators.push_back(static_cast<OperatorIR*>(Get(i)));
    }
  }
  return operators;
}

Status IRNode::CopyFromNode(const IRNode* node,
                            absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  line_ = node->line_;
  col_ = node->col_;
  line_col_set_ = node->line_col_set_;
  ast_node_ = node->ast_node_;
  return CopyFromNodeImpl(node, copied_nodes_map);
}

void IRNode::SetLineCol(int64_t line, int64_t col) {
  line_ = line;
  col_ = col;
  line_col_set_ = true;
}
void IRNode::SetLineCol(const pypa::AstPtr& ast_node) {
  ast_node_ = ast_node;
  SetLineCol(ast_node->line, ast_node->column);
}

Status OperatorIR::AddParent(OperatorIR* parent) {
  DCHECK(can_have_parents_);
  DCHECK(!graph_ptr()->dag().HasEdge(parent->id(), id()))
      << absl::Substitute("Edge between parent op $0(id=$1) and child op $2(id=$3) exists.",
                          parent->type_string(), parent->id(), type_string(), id());

  parents_.push_back(parent);
  return graph_ptr()->AddEdge(parent, this);
}

Status OperatorIR::RemoveParent(OperatorIR* parent) {
  DCHECK(graph_ptr()->dag().HasEdge(parent->id(), id()))
      << absl::Substitute("Edge between parent op $0(id=$1) and child op $2(id=$3) does not exist.",
                          parent->type_string(), parent->id(), type_string(), id());
  parents_.erase(std::remove(parents_.begin(), parents_.end(), parent), parents_.end());
  return graph_ptr()->DeleteEdge(parent->id(), id());
}

Status OperatorIR::ReplaceParent(OperatorIR* old_parent, OperatorIR* new_parent) {
  DCHECK(graph_ptr()->dag().HasEdge(old_parent->id(), id()))
      << absl::Substitute("Edge between parent op $0 and child op $1 does not exist.",
                          old_parent->DebugString(), DebugString());
  for (size_t i = 0; i < parents_.size(); ++i) {
    if (parents_[i] == old_parent) {
      parents_[i] = new_parent;
      graph_ptr()->dag().ReplaceParentEdge(id(), old_parent->id(), new_parent->id());
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
                        graph_ptr()->CreateNode<MapIR>(ast_node(), parent, ColExpressionVector{},
                                                       /*keep_input_columns*/ true));
    new_parents.push_back(map);
  }
  return new_parents;
}

Status OperatorIR::PruneOutputColumnsTo(const absl::flat_hash_set<std::string>& output_colnames) {
  DCHECK(IsRelationInit());
  for (const auto& kept_colname : output_colnames) {
    DCHECK(relation_.HasColumn(kept_colname));
  }

  auto output_cols = output_colnames;
  // Ensure that we always output at least one column, unless it's a memory sink,
  // in which case it's ok to output 0 columns.
  if (!Match(this, MemorySink())) {
    if (!output_cols.size()) {
      output_cols.insert(relation().col_names()[0]);
    }
  }
  PL_ASSIGN_OR_RETURN(auto required_columns, PruneOutputColumnsToImpl(output_cols));
  Relation updated_relation;
  for (const auto& colname : relation_.col_names()) {
    if (required_columns.contains(colname)) {
      updated_relation.AddColumn(relation_.GetColumnType(colname), colname,
                                 relation_.GetColumnDesc(colname));
    }
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

  return Status::OK();
}

std::string DebugStringFmt(int64_t depth, std::string name,
                           std::map<std::string, std::string> property_value_map) {
  std::vector<std::string> property_strings;
  std::map<std::string, std::string>::iterator it;
  std::string depth_string = std::string(depth, '\t');
  property_strings.push_back(absl::Substitute("$0$1", depth_string, name));

  for (it = property_value_map.begin(); it != property_value_map.end(); it++) {
    std::string prop_str = absl::Substitute("$0 $1\t-$2", depth_string, it->first, it->second);
    property_strings.push_back(prop_str);
  }
  return absl::StrJoin(property_strings, "\n");
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
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, old_start_time_expr));
  }
  if (end_time_expr_) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, old_end_time_expr));
  }

  PL_ASSIGN_OR_RETURN(start_time_expr_,
                      graph_ptr()->OptionallyCloneWithEdge(this, new_start_time_expr));
  PL_ASSIGN_OR_RETURN(end_time_expr_,
                      graph_ptr()->OptionallyCloneWithEdge(this, new_end_time_expr));

  if (old_start_time_expr) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(old_start_time_expr->id()));
  }
  if (old_end_time_expr) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(old_end_time_expr->id()));
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

  return Status::OK();
}

Status MapIR::SetColExprs(const ColExpressionVector& exprs) {
  auto old_exprs = col_exprs_;
  col_exprs_.clear();
  for (const ColumnExpression& expr : old_exprs) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, expr.node));
  }
  for (const auto& expr : exprs) {
    PL_RETURN_IF_ERROR(AddColExpr(expr));
  }
  for (const ColumnExpression& expr : old_exprs) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(expr.node->id()));
  }
  return Status::OK();
}

Status MapIR::AddColExpr(const ColumnExpression& expr) {
  PL_ASSIGN_OR_RETURN(auto expr_node, graph_ptr()->OptionallyCloneWithEdge(this, expr.node));
  col_exprs_.emplace_back(expr.name, expr_node);
  return Status::OK();
}

Status MapIR::UpdateColExpr(std::string_view name, ExpressionIR* expr) {
  PL_ASSIGN_OR_RETURN(auto expr_node, graph_ptr()->OptionallyCloneWithEdge(this, expr));
  for (size_t i = 0; i < col_exprs_.size(); ++i) {
    if (col_exprs_[i].name == name) {
      auto old_expr = col_exprs_[i].node;
      col_exprs_[i].node = expr_node;
      PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, old_expr));
      return graph_ptr()->DeleteOrphansInSubtree(old_expr->id());
    }
  }
  return error::Internal("Column $0 does not exist in Map", name);
}

Status MapIR::Init(OperatorIR* parent, const ColExpressionVector& col_exprs,
                   bool keep_input_columns) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  PL_RETURN_IF_ERROR(SetColExprs(col_exprs));
  keep_input_columns_ = keep_input_columns;
  return Status::OK();
}

StatusOr<absl::flat_hash_set<std::string>> CollectInputColumns(const ExpressionIR* expr) {
  if (Match(expr, DataNode())) {
    return absl::flat_hash_set<std::string>{};
  }
  if (Match(expr, ColumnNode())) {
    return absl::flat_hash_set<std::string>{static_cast<const ColumnIR*>(expr)->col_name()};
  }
  if (Match(expr, MetadataLiteral())) {
    return CollectInputColumns(static_cast<const MetadataLiteralIR*>(expr)->literal());
  }
  if (!Match(expr, Func())) {
    return error::Internal("Unexpected Expression type: $0", expr->DebugString());
  }
  absl::flat_hash_set<std::string> ret;
  auto func = static_cast<const FuncIR*>(expr);
  for (const ExpressionIR* arg : func->args()) {
    PL_ASSIGN_OR_RETURN(auto input_cols, CollectInputColumns(arg));
    ret.insert(input_cols.begin(), input_cols.end());
  }
  return ret;
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> MapIR::RequiredInputColumns() const {
  absl::flat_hash_set<std::string> required;
  for (const auto& expr : col_exprs_) {
    PL_ASSIGN_OR_RETURN(auto inputs, CollectInputColumns(expr.node));
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

Status FilterIR::SetFilterExpr(ExpressionIR* expr) {
  auto old_filter_expr = filter_expr_;
  if (old_filter_expr) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, old_filter_expr));
  }
  PL_ASSIGN_OR_RETURN(filter_expr_, graph_ptr()->OptionallyCloneWithEdge(this, expr));
  if (old_filter_expr) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(old_filter_expr->id()));
  }
  return Status::OK();
}

absl::flat_hash_set<std::string> ColumnsFromRelation(Relation r) {
  auto col_names = r.col_names();
  return {col_names.begin(), col_names.end()};
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> FilterIR::RequiredInputColumns() const {
  DCHECK(IsRelationInit());
  return std::vector<absl::flat_hash_set<std::string>>{ColumnsFromRelation(relation())};
}

StatusOr<absl::flat_hash_set<std::string>> FilterIR::PruneOutputColumnsToImpl(
    const absl::flat_hash_set<std::string>& output_cols) {
  PL_ASSIGN_OR_RETURN(auto filter_cols, CollectInputColumns(filter_expr_));
  auto filter_and_outputs = output_cols;
  filter_and_outputs.insert(filter_cols.begin(), filter_cols.end());
  return filter_and_outputs;
}

Status FilterIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_filter_op();
  op->set_op_type(planpb::FILTER_OPERATOR);
  DCHECK_EQ(parents().size(), 1UL);

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parents()[0]->id());
    col_pb->set_index(i);
  }

  auto expr = pb->mutable_expression();
  PL_RETURN_IF_ERROR(filter_expr_->ToProto(expr));
  return Status::OK();
}

Status LimitIR::Init(OperatorIR* parent, int64_t limit_value) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  SetLimitValue(limit_value);
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

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parents()[0]->id());
    col_pb->set_index(i);
  }
  if (!limit_value_set_) {
    return CreateIRNodeError("Limit value not set properly.");
  }

  pb->set_limit(limit_value_);
  return Status::OK();
}

Status BlockingAggIR::Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups,
                           const ColExpressionVector& agg_expr) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  PL_RETURN_IF_ERROR(SetGroups(groups));
  return SetAggExprs(agg_expr);
}

Status BlockingAggIR::SetGroups(const std::vector<ColumnIR*>& groups) {
  DCHECK(groups_.empty());
  groups_.resize(groups.size());
  for (size_t i = 0; i < groups.size(); ++i) {
    PL_ASSIGN_OR_RETURN(groups_[i], graph_ptr()->OptionallyCloneWithEdge(this, groups[i]));
  }
  return Status::OK();
}

Status BlockingAggIR::SetAggExprs(const ColExpressionVector& agg_exprs) {
  auto old_agg_expressions = aggregate_expressions_;
  for (const ColumnExpression& agg_expr : aggregate_expressions_) {
    ExpressionIR* expr = agg_expr.node;
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, expr));
  }
  aggregate_expressions_.clear();

  for (const auto& agg_expr : agg_exprs) {
    PL_ASSIGN_OR_RETURN(auto updated_expr,
                        graph_ptr()->OptionallyCloneWithEdge(this, agg_expr.node));
    aggregate_expressions_.emplace_back(agg_expr.name, updated_expr);
  }

  for (const auto& old_agg_expr : old_agg_expressions) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(old_agg_expr.node->id()));
  }

  return Status::OK();
}

StatusOr<std::vector<absl::flat_hash_set<std::string>>> BlockingAggIR::RequiredInputColumns()
    const {
  absl::flat_hash_set<std::string> required;
  for (const auto& group : groups_) {
    required.insert(group->col_name());
  }
  for (const auto& agg_expr : aggregate_expressions_) {
    PL_ASSIGN_OR_RETURN(auto ret, CollectInputColumns(agg_expr.node));
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
  for (const ColumnIR* group : groups_) {
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
    PL_ASSIGN_OR_RETURN(groups_[i], graph_ptr()->OptionallyCloneWithEdge(this, groups[i]));
  }
  return Status::OK();
}

Status GroupByIR::CopyFromNodeImpl(const IRNode* source,
                                   absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const GroupByIR* group_by = static_cast<const GroupByIR*>(source);
  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : group_by->groups_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_column, graph_ptr()->CopyNode(column, copied_nodes_map));
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
  for (types::DataType dt : casted_ir.args_types()) {
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

  for (const auto& agg_expr : aggregate_expressions_) {
    auto expr = pb->add_values();
    PL_RETURN_IF_ERROR(EvaluateAggregateExpression(expr, *agg_expr.node));
    pb->add_value_names(agg_expr.name);
  }

  for (ColumnIR* group : groups_) {
    auto group_pb = pb->add_groups();
    PL_RETURN_IF_ERROR(group->ToProto(group_pb));
    pb->add_group_names(group->col_name());
  }

  // TODO(nserrino/philkuz): Add support for streaming aggregates in the compiler.
  pb->set_windowed(false);

  op->set_op_type(planpb::AGGREGATE_OPERATOR);
  return Status::OK();
}

Status DataIR::ToProto(planpb::ScalarExpression* expr) const {
  auto value_pb = expr->mutable_constant();
  return ToProto(value_pb);
}

Status DataIR::ToProto(planpb::ScalarValue* value_pb) const {
  value_pb->set_data_type(evaluated_data_type_);
  return ToProtoImpl(value_pb);
}

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
    default: {
      return error::InvalidArgument("Error processing $0: $1 not handled as a default data type.",
                                    name, magic_enum::enum_name(value.data_type()));
    }
  }
}

Status IntIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_int64_value(val_);
  return Status::OK();
}

Status FloatIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_float64_value(val_);
  return Status::OK();
}

Status BoolIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_bool_value(val_);
  return Status::OK();
}

Status StringIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_string_value(str_);
  return Status::OK();
}

Status TimeIR::ToProtoImpl(planpb::ScalarValue* value) const {
  value->set_time64_ns_value(val_);
  return Status::OK();
}

Status UInt128IR::ToProtoImpl(planpb::ScalarValue* value) const {
  auto uint128pb = value->mutable_uint128_value();
  uint128pb->set_high(absl::Uint128High64(val_));
  uint128pb->set_low(absl::Uint128Low64(val_));
  return Status::OK();
}

Status ColumnIR::Init(const std::string& col_name, int64_t parent_idx) {
  SetColumnName(col_name);
  SetContainingOperatorParentIdx(parent_idx);
  return Status::OK();
}

StatusOr<int64_t> ColumnIR::GetColumnIndex() const {
  PL_ASSIGN_OR_RETURN(auto op, ReferencedOperator());
  if (!op->relation().HasColumn(col_name())) {
    return CreateIRNodeError("Column '$0' does not exist in relation $1", col_name(),
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

void ColumnIR::SetContainingOperatorParentIdx(int64_t container_op_parent_idx) {
  DCHECK_GE(container_op_parent_idx, 0);
  container_op_parent_idx_ = container_op_parent_idx;
  container_op_parent_idx_set_ = true;
}

StatusOr<std::vector<OperatorIR*>> ColumnIR::ContainingOperators() const {
  std::vector<OperatorIR*> parents;
  IR* graph = graph_ptr();
  std::queue<int64_t> cur_ids;
  cur_ids.push(id());

  while (cur_ids.size()) {
    auto cur_id = cur_ids.front();
    cur_ids.pop();
    IRNode* cur_node = graph->Get(cur_id);
    if (cur_node->IsOperator()) {
      parents.push_back(static_cast<OperatorIR*>(cur_node));
      continue;
    }
    std::vector<int64_t> parents = graph->dag().ParentsOf(cur_id);
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

Status CollectionIR::Init(const std::vector<IRNode*>& children) { return SetChildren(children); }

Status CollectionIR::SetChildren(const std::vector<IRNode*>& children) {
  if (!children_.empty()) {
    return CreateIRNodeError(
        "CollectionIR already has children and likely has been created already.");
  }
  children_ = children;
  for (size_t i = 0; i < children_.size(); ++i) {
    PL_ASSIGN_OR_RETURN(children_[i], graph_ptr()->OptionallyCloneWithEdge(this, children_[i]));
  }
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
  args_.push_back(arg);
  return graph_ptr()->AddEdge(this, arg);
}

Status FuncIR::UpdateArg(int64_t idx, ExpressionIR* arg) {
  CHECK_LT(idx, static_cast<int64_t>(args_.size()))
      << "Tried to update arg of index greater than number of args.";
  ExpressionIR* old_arg = args_[idx];
  args_[idx] = arg;
  PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, old_arg));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, arg));
  PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(old_arg->id()));
  return Status::OK();
}

Status FuncIR::AddOrCloneArg(ExpressionIR* arg) {
  if (arg == nullptr) {
    return error::Internal("Argument for FuncIR is null.");
  }
  PL_ASSIGN_OR_RETURN(auto updated_arg, graph_ptr()->OptionallyCloneWithEdge(this, arg));
  args_.push_back(updated_arg);
  return Status::OK();
}

Status FuncIR::ToProto(planpb::ScalarExpression* expr) const {
  auto func_pb = expr->mutable_func();
  func_pb->set_name(func_name());
  func_pb->set_id(func_id_);
  for (ExpressionIR* arg : args()) {
    PL_RETURN_IF_ERROR(arg->ToProto(func_pb->add_args()));
  }
  if (args_types().size() != args().size()) {
    return CreateIRNodeError("arg types not resolved");
  }
  for (types::DataType dt : args_types()) {
    func_pb->add_args_data_types(dt);
  }
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

Status MetadataIR::ResolveMetadataColumn(MetadataResolverIR* resolver_op,
                                         MetadataProperty* property) {
  SetColumnName(property->GetColumnRepr());
  resolver_ = resolver_op;
  property_ = property;
  has_metadata_resolver_ = true;
  return Status::OK();
}

/* MetadataLiteral IR */
Status MetadataLiteralIR::Init(DataIR* literal) { return SetLiteral(literal); }

Status MetadataLiteralIR::SetLiteral(DataIR* literal) {
  DCHECK_EQ(literal_, nullptr);
  PL_ASSIGN_OR_RETURN(literal_, graph_ptr()->OptionallyCloneWithEdge(this, literal));
  return Status::OK();
}

bool MetadataResolverIR::HasMetadataColumn(const std::string& col_name) {
  auto md_map_it = metadata_columns_.find(col_name);
  return md_map_it != metadata_columns_.end();
}

Status MetadataLiteralIR::ToProto(planpb::ScalarExpression* expr) const {
  return literal_->ToProto(expr);
}

Status MetadataResolverIR::AddMetadata(MetadataProperty* md_property) {
  // Check to make sure that name is a valid attribute name
  // TODO(philkuz) grab the type from metadata handler, if it's invalid, it'll return an error.
  // Check if metadata column exists
  if (HasMetadataColumn(md_property->name())) {
    return Status::OK();
  }

  metadata_columns_.emplace(md_property->name(), md_property);
  return Status::OK();
}

Status OperatorIR::CopyFromNode(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PL_RETURN_IF_ERROR(IRNode::CopyFromNode(node, copied_nodes_map));
  const OperatorIR* source = static_cast<const OperatorIR*>(node);

  is_source_ = source->is_source_;
  relation_ = source->relation_;
  relation_init_ = source->relation_init_;
  return Status::OK();
}

// Clone Functions
StatusOr<std::unique_ptr<IR>> IR::Clone() const {
  auto new_ir = std::make_unique<IR>();
  // Iterate through the children.
  absl::flat_hash_map<const IRNode*, IRNode*> copied_nodes_map;
  for (int64_t i : dag().TopologicalSort()) {
    IRNode* node = Get(i);
    if (new_ir->HasNode(i) && new_ir->Get(i)->type() == node->type()) {
      continue;
    }
    PL_ASSIGN_OR_RETURN(IRNode * new_node, new_ir->CopyNode(node, &copied_nodes_map));
    if (new_node->IsOperator()) {
      PL_RETURN_IF_ERROR(static_cast<OperatorIR*>(new_node)->CopyParentsFrom(
          static_cast<const OperatorIR*>(node)));
    }
  }
  // TODO(philkuz) check to make sure these are the same.
  new_ir->dag_ = dag_;
  return new_ir;
}

Status OperatorIR::CopyParentsFrom(const OperatorIR* source_op) {
  DCHECK(parents_.empty());
  for (const auto& parent : source_op->parents()) {
    IRNode* new_parent = graph_ptr()->Get(parent->id());
    DCHECK(Match(new_parent, Operator()));
    PL_RETURN_IF_ERROR(AddParent(static_cast<OperatorIR*>(new_parent)));
  }
  return Status::OK();
}

std::vector<OperatorIR*> OperatorIR::Children() const {
  plan::DAG dag = graph_ptr()->dag();
  std::vector<OperatorIR*> op_children;
  for (int64_t d : dag.DependenciesOf(id())) {
    auto ir_node = graph_ptr()->Get(d);
    if (ir_node->IsOperator()) {
      op_children.push_back(static_cast<OperatorIR*>(ir_node));
    }
  }
  return op_children;
}

Status ColumnIR::CopyFromNode(const IRNode* source,
                              absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  PL_RETURN_IF_ERROR(IRNode::CopyFromNode(source, copied_nodes_map));
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

Status CollectionIR::CopyFromCollection(
    const CollectionIR* source, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  std::vector<IRNode*> new_children;
  for (const IRNode* child : source->children()) {
    PL_ASSIGN_OR_RETURN(IRNode * new_child, graph_ptr()->CopyNode(child, copied_nodes_map));
    new_children.push_back(new_child);
  }
  return SetChildren(new_children);
}

Status ListIR::CopyFromNodeImpl(const IRNode* source,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  return CopyFromCollection(static_cast<const ListIR*>(source), copied_nodes_map);
}

Status TupleIR::CopyFromNodeImpl(const IRNode* source,
                                 absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  return CopyFromCollection(static_cast<const TupleIR*>(source), copied_nodes_map);
}

Status FuncIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const FuncIR* func = static_cast<const FuncIR*>(node);
  func_prefix_ = func->func_prefix_;
  op_ = func->op_;
  func_name_ = func->func_name_;
  args_types_ = func->args_types_;
  func_id_ = func->func_id_;
  evaluated_data_type_ = func->evaluated_data_type_;
  is_data_type_evaluated_ = func->is_data_type_evaluated_;

  for (const ExpressionIR* arg : func->args_) {
    // auto id = arg->id();
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_arg, graph_ptr()->CopyNode(arg, copied_nodes_map));
    PL_RETURN_IF_ERROR(AddArg(new_arg));
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

Status MetadataLiteralIR::CopyFromNodeImpl(
    const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const MetadataLiteralIR* literal_ir = static_cast<const MetadataLiteralIR*>(node);
  PL_ASSIGN_OR_RETURN(DataIR * new_literal,
                      graph_ptr()->CopyNode(literal_ir->literal_, copied_nodes_map));
  return SetLiteral(new_literal);
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

  if (has_time_expressions_) {
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_start_expr,
                        graph_ptr()->CopyNode(source_ir->start_time_expr_, copied_nodes_map));
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_stop_expr,
                        graph_ptr()->CopyNode(source_ir->end_time_expr_, copied_nodes_map));
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

Status MetadataResolverIR::CopyFromNodeImpl(const IRNode* node,
                                            absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const MetadataResolverIR* resolver_ir = static_cast<const MetadataResolverIR*>(node);
  metadata_columns_ = resolver_ir->metadata_columns_;
  return Status::OK();
}

Status MapIR::CopyFromNodeImpl(const IRNode* node,
                               absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const MapIR* map_ir = static_cast<const MapIR*>(node);
  ColExpressionVector new_col_exprs;
  for (const ColumnExpression& col_expr : map_ir->col_exprs_) {
    PL_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                        graph_ptr()->CopyNode(col_expr.node, copied_nodes_map));
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
                        graph_ptr()->CopyNode(col_expr.node, copied_nodes_map));
    new_agg_exprs.push_back({col_expr.name, new_node});
  }

  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : blocking_agg->groups_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_column, graph_ptr()->CopyNode(column, copied_nodes_map));
    new_groups.push_back(new_column);
  }

  PL_RETURN_IF_ERROR(SetAggExprs(new_agg_exprs));
  PL_RETURN_IF_ERROR(SetGroups(new_groups));

  return Status::OK();
}

Status FilterIR::CopyFromNodeImpl(const IRNode* node,
                                  absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const FilterIR* filter = static_cast<const FilterIR*>(node);
  PL_ASSIGN_OR_RETURN(ExpressionIR * new_node,
                      graph_ptr()->CopyNode(filter->filter_expr_, copied_nodes_map));
  PL_RETURN_IF_ERROR(SetFilterExpr(new_node));
  return Status::OK();
}

Status LimitIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const LimitIR* limit = static_cast<const LimitIR*>(node);
  limit_value_ = limit->limit_value_;
  limit_value_set_ = limit->limit_value_set_;
  return Status::OK();
}

Status GRPCSinkIR::CopyFromNodeImpl(const IRNode* node,
                                    absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const GRPCSinkIR* grpc_sink = static_cast<const GRPCSinkIR*>(node);
  destination_id_ = grpc_sink->destination_id_;
  destination_address_ = grpc_sink->destination_address_;
  return Status::OK();
}

Status GRPCSourceGroupIR::CopyFromNodeImpl(const IRNode* node,
                                           absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const GRPCSourceGroupIR* grpc_source_group = static_cast<const GRPCSourceGroupIR*>(node);
  source_id_ = grpc_source_group->source_id_;
  grpc_address_ = grpc_source_group->grpc_address_;
  dependent_sinks_ = grpc_source_group->dependent_sinks_;
  return Status::OK();
}

Status GRPCSourceIR::CopyFromNodeImpl(const IRNode*, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  return Status::OK();
}

Status UnionIR::CopyFromNodeImpl(const IRNode* node, absl::flat_hash_map<const IRNode*, IRNode*>*) {
  const UnionIR* union_node = static_cast<const UnionIR*>(node);
  column_mappings_ = union_node->column_mappings_;
  return Status::OK();
}

Status JoinIR::CopyFromNodeImpl(const IRNode* node,
                                absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const JoinIR* join_node = static_cast<const JoinIR*>(node);
  join_type_ = join_node->join_type_;

  std::vector<ColumnIR*> new_output_columns;
  for (const ColumnIR* col : join_node->output_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph_ptr()->CopyNode(col, copied_nodes_map));
    new_output_columns.push_back(new_node);
  }
  PL_RETURN_IF_ERROR(SetOutputColumns(join_node->column_names_, new_output_columns));

  std::vector<ColumnIR*> new_left_columns;
  for (const ColumnIR* col : join_node->left_on_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph_ptr()->CopyNode(col, copied_nodes_map));
    new_left_columns.push_back(new_node);
  }

  std::vector<ColumnIR*> new_right_columns;
  for (const ColumnIR* col : join_node->right_on_columns_) {
    PL_ASSIGN_OR_RETURN(ColumnIR * new_node, graph_ptr()->CopyNode(col, copied_nodes_map));
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
  auto pb = op->mutable_grpc_sink_op();
  op->set_op_type(planpb::GRPC_SINK_OPERATOR);
  pb->set_address(destination_address());
  pb->set_destination_id(destination_id());
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

StatusOr<planpb::Plan> IR::ToProto() const {
  auto plan = planpb::Plan();
  // TODO(michelle) For M1.5 , we'll only handle plans with a single plan fragment. In the future
  // we will need to update this to loop through all plan fragments.
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
      PL_RETURN_IF_ERROR(OutputProto(plan_fragment, static_cast<OperatorIR*>(node)));
    } else {
      non_op_nodes.emplace(node_id);
    }
  }
  dag_.ToProto(plan_fragment->mutable_dag(), non_op_nodes);
  return plan;
}

Status IR::OutputProto(planpb::PlanFragment* pf, const OperatorIR* op_node) const {
  // Check to make sure that the relation is set for this op_node, otherwise it's not connected to
  // a Sink.
  if (!op_node->IsRelationInit()) {
    return op_node->CreateIRNodeError("$0 doesn't have a relation.", op_node->DebugString());
  }

  // Add PlanNode.
  auto plan_node = pf->add_nodes();
  plan_node->set_id(op_node->id());
  auto op_pb = plan_node->mutable_op();
  PL_RETURN_IF_ERROR(op_node->ToProto(op_pb));

  return Status::OK();
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

std::vector<IRNode*> IR::FindNodesOfType(IRNodeType type) const {
  std::vector<IRNode*> nodes;
  for (auto& i : dag().TopologicalSort()) {
    IRNode* node = Get(i);
    if (node->type() == type) {
      nodes.push_back(node);
    }
  }
  return nodes;
}
Status GRPCSourceGroupIR::AddGRPCSink(GRPCSinkIR* sink_op) {
  if (sink_op->destination_id() != source_id_) {
    return DExitOrIRNodeError("Source id $0 and destination id $1 aren't equal.",
                              sink_op->destination_id(), source_id_);
  }
  if (!GRPCAddressSet()) {
    return DExitOrIRNodeError("$0 doesn't have a physical agent associated with it.",
                              DebugString());
  }
  sink_op->SetDestinationAddress(grpc_address_);
  dependent_sinks_.emplace_back(sink_op);
  return Status::OK();
}

Status UnionIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_union_op();
  op->set_op_type(planpb::UNION_OPERATOR);

  auto types = relation().col_types();
  auto names = relation().col_names();
  DCHECK_EQ(parents().size(), column_mappings_.size()) << "parents and column_mappings disagree.";

  for (const auto& column_mapping : column_mappings_) {
    auto* pb_column_mapping = pb->add_column_mappings();
    for (const ColumnIR* col : column_mapping) {
      PL_ASSIGN_OR_RETURN(auto index, col->GetColumnIndex());
      pb_column_mapping->add_column_indexes(index);
    }
  }

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_names(names[i]);
  }

  // NOTE: not setting value as this is set in the execution engine. Keeping this here in case it
  // needs to be modified in the future.
  // pb->set_rows_per_batch(1024);

  return Status::OK();
}

Status UnionIR::AddColumnMapping(const InputColumnMapping& column_mapping) {
  InputColumnMapping cloned_mapping;
  for (ColumnIR* col : column_mapping) {
    PL_ASSIGN_OR_RETURN(auto cloned, graph_ptr()->OptionallyCloneWithEdge(this, col));
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
      PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, col));
    }
  }
  for (const auto& new_mapping : column_mappings) {
    PL_RETURN_IF_ERROR(AddColumnMapping(new_mapping));
  }
  for (const auto& old_mapping : old_mappings) {
    for (ColumnIR* col : old_mapping) {
      PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(col->id()));
    }
  }
  return Status::OK();
}

// TODO(nserrino, philkuz): Once we support null values in columns, we can support performing a
// union where the output schema of the union is the combined set of the input relations, and any
// input table that is missing a certain column will just output null.
Status UnionIR::SetRelationFromParents() {
  DCHECK(!parents().empty());

  std::vector<Relation> relations;
  OperatorIR* base_parent = parents()[0];
  Relation base_relation = base_parent->relation();
  PL_RETURN_IF_ERROR(SetRelation(base_relation));

  std::vector<InputColumnMapping> mappings;
  for (const auto& [parent_idx, parent] : Enumerate(parents())) {
    Relation cur_relation = parent->relation();
    std::string err_msg = absl::Substitute(
        "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
        base_parent->DebugString(), parent->DebugString(), DebugString(),
        base_relation.DebugString(), cur_relation.DebugString(), "$0");
    if (cur_relation.NumColumns() != base_relation.NumColumns()) {
      return CreateIRNodeError(err_msg, "Column count wrong.");
    }
    InputColumnMapping column_mapping;
    for (const std::string& col_name : base_relation.col_names()) {
      types::DataType col_type = base_relation.GetColumnType(col_name);
      if (!cur_relation.HasColumn(col_name) || cur_relation.GetColumnType(col_name) != col_type) {
        return CreateIRNodeError(err_msg,
                                 absl::Substitute("Missing or wrong type for $0.", col_name));
      }
      PL_ASSIGN_OR_RETURN(auto col_node, graph_ptr()->CreateNode<ColumnIR>(
                                             ast_node(), col_name, /*parent_op_idx*/ parent_idx));
      column_mapping.push_back(col_node);
    }
    mappings.push_back(column_mapping);
  }
  return SetColumnMappings(mappings);
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
                        graph_ptr()->OptionallyCloneWithEdge(this, left_columns[i]));
  }
  right_on_columns_.resize(right_columns.size());
  for (size_t i = 0; i < right_columns.size(); ++i) {
    PL_ASSIGN_OR_RETURN(right_on_columns_[i],
                        graph_ptr()->OptionallyCloneWithEdge(this, right_columns[i]));
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
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, old_col));
  }
  for (auto new_col : output_columns_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, new_col));
  }
  for (auto old_col : old_output_cols) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteOrphansInSubtree(old_col->id()));
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
    DCHECK(!Match(value, Collection())) << "Collections not supported in UDTF";
    if (!value->IsData()) {
      return CreateIRNodeError("expected scalar value, received '$0'", value->type_string());
    }
    PL_ASSIGN_OR_RETURN(arg_values_[idx],
                        graph_ptr()->OptionallyCloneWithEdge(this, static_cast<DataIR*>(value)));
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

// StatusOr<IRNode*> UDTFSourceIR::DeepCloneIntoImpl(IR* graph) const {
Status UDTFSourceIR::CopyFromNodeImpl(
    const IRNode* source, absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) {
  const UDTFSourceIR* udtf = static_cast<const UDTFSourceIR*>(source);
  func_name_ = udtf->func_name_;
  udtf_spec_ = udtf->udtf_spec_;
  std::vector<ExpressionIR*> arg_values;
  for (const DataIR* arg_element : udtf->arg_values_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_arg_element,
                        graph_ptr()->CopyNode(arg_element, copied_nodes_map));
    DCHECK(Match(new_arg_element, DataNode()));
    arg_values.push_back(static_cast<DataIR*>(new_arg_element));
  }
  return SetArgValues(arg_values);
}

bool OperatorIR::NodeMatches(IRNode* node) { return Match(node, Operator()); }
bool StringIR::NodeMatches(IRNode* node) { return Match(node, String()); }
bool IntIR::NodeMatches(IRNode* node) { return Match(node, Int()); }
bool ListIR::NodeMatches(IRNode* node) { return Match(node, List()); }
bool TupleIR::NodeMatches(IRNode* node) { return Match(node, Tuple()); }
bool FuncIR::NodeMatches(IRNode* node) { return Match(node, Func()); }
bool ExpressionIR::NodeMatches(IRNode* node) { return Match(node, Expression()); }

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
