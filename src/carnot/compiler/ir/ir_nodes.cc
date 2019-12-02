#include <queue>

#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"

namespace pl {
namespace carnot {
namespace compiler {

using table_store::schema::Relation;

Status IR::AddEdge(int64_t from_node, int64_t to_node) {
  dag_.AddEdge(from_node, to_node);
  return Status::OK();
}

Status IR::AddEdge(IRNode* from_node, IRNode* to_node) {
  return AddEdge(from_node->id(), to_node->id());
}

bool IR::HasEdge(IRNode* from_node, IRNode* to_node) {
  return HasEdge(from_node->id(), to_node->id());
}

bool IR::HasEdge(int64_t from_node, int64_t to_node) { return dag_.HasEdge(from_node, to_node); }

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

Status IR::DeleteNode(int64_t node) {
  if (!dag_.HasNode(node)) {
    return error::InvalidArgument("No node $0 exists in graph.", node);
  }
  dag_.DeleteNode(node);
  return Status::OK();
}

Status IR::DeleteNodeAndChildren(int64_t node) {
  if (dag_.ParentsOf(node).size() != 0) {
    // TODO(philkuz) if this errors out unexpectedly, it's because you used a non-tree dag.
    return Get(node)->CreateIRNodeError("$0 still is a child of $1.", Get(node)->DebugString(),
                                        absl::StrJoin(dag_.ParentsOf(node), ","));
  }
  for (int64_t child_node : dag_.DependenciesOf(node)) {
    PL_RETURN_IF_ERROR(DeleteEdge(node, child_node));
    PL_RETURN_IF_ERROR(DeleteNodeAndChildren(child_node));
  }
  return DeleteNode(node);
}

std::string IR::DebugString() {
  std::string debug_string = dag().DebugString() + "\n";
  for (auto const& a : id_node_map_) {
    debug_string += a.second->DebugString() + "\n";
  }
  return debug_string;
}

StatusOr<IRNode*> IRNode::DeepCloneInto(IR* graph) const {
  DCHECK(!graph->HasNode(id())) << absl::Substitute(
      "Cannot clone $0. Target graph already has $1 with id $2", DebugString(),
      graph->Get(id())->DebugString(), id());
  PL_ASSIGN_OR_RETURN(IRNode * other, DeepCloneIntoImpl(graph));
  DCHECK_EQ(other->id_, id_) << absl::Substitute(
      "ids disagree for $0. Use MakeNode(int) instead of MakeNode()", other->type_string());
  other->line_ = line_;
  other->col_ = col_;
  other->line_col_set_ = line_col_set_;
  other->ast_node_ = ast_node_;
  return other;
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
  name_set_ = true;
  out_columns_ = out_columns;
  return Status::OK();
}

Status MemorySourceIR::Init(const std::string& table_name,
                            const std::vector<std::string>& select_columns) {
  table_name_ = table_name;
  column_names_ = select_columns;
  return Status::OK();
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

Status RangeIR::Init(OperatorIR* parent_node, IRNode* start_repr, IRNode* stop_repr) {
  if (parent_node->type() != IRNodeType::kMemorySource) {
    return CreateIRNodeError("Expected parent of Range to be a Memory Source, not a $0.",
                             parent_node->type_string());
  }
  PL_RETURN_IF_ERROR(AddParent(parent_node));
  return SetStartStop(start_repr, stop_repr);
}

Status RangeIR::SetStartStop(IRNode* start_repr, IRNode* stop_repr) {
  if (start_repr_ != nullptr) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(id(), start_repr_->id()));
  }
  if (stop_repr_ != nullptr) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(id(), stop_repr_->id()));
  }
  start_repr_ = start_repr;
  stop_repr_ = stop_repr;
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, start_repr_));
  return graph_ptr()->AddEdge(this, stop_repr_);
}

Status RangeIR::ToProto(planpb::Operator*) const {
  return error::Unimplemented("$0 does not have a protobuf.", type_string());
}

Status MapIR::SetColExprs(const ColExpressionVector& exprs) {
  for (const ColumnExpression& mapped_expression : col_exprs_) {
    ExpressionIR* expr = mapped_expression.node;
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, expr));
  }
  col_exprs_ = exprs;
  for (const ColumnExpression& mapped_expression : col_exprs_) {
    ExpressionIR* expr = mapped_expression.node;
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, expr));
  }
  return Status::OK();
}

Status MapIR::Init(OperatorIR* parent, const ColExpressionVector& col_exprs,
                   bool keep_input_columns) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  PL_RETURN_IF_ERROR(SetColExprs(col_exprs));
  keep_input_columns_ = keep_input_columns;
  return Status::OK();
}

Status DropIR::Init(OperatorIR* parent, const std::vector<std::string>& drop_cols) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  col_names_ = drop_cols;
  return Status::OK();
}

Status DropIR::ToProto(planpb::Operator*) const {
  return error::Unimplemented("$0 does not have a protobuf.", type_string());
}

Status OperatorIR::EvaluateExpression(planpb::ScalarExpression* expr, const IRNode& ir_node) const {
  switch (ir_node.type()) {
    case IRNodeType::kMetadata:
    case IRNodeType::kColumn: {
      const ColumnIR& col_ir = static_cast<const ColumnIR&>(ir_node);
      auto col = expr->mutable_column();
      PL_ASSIGN_OR_RETURN(int64_t ref_op_id, col_ir.ReferenceID());
      col->set_node(ref_op_id);
      col->set_index(col_ir.col_idx());
      break;
    }
    case IRNodeType::kFunc: {
      auto func = expr->mutable_func();
      auto casted_ir = static_cast<const FuncIR&>(ir_node);
      func->set_name(casted_ir.func_name());
      for (const auto& arg : casted_ir.args()) {
        auto func_arg = func->add_args();
        PL_RETURN_IF_ERROR(EvaluateExpression(func_arg, *arg));
      }
      func->set_id(casted_ir.func_id());
      for (const types::DataType dt : casted_ir.args_types()) {
        func->add_args_data_types(dt);
      }
      break;
    }
    case IRNodeType::kInt: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const IntIR&>(ir_node);
      value->set_data_type(types::DataType::INT64);
      value->set_int64_value(casted_ir.val());
      break;
    }
    case IRNodeType::kString: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const StringIR&>(ir_node);
      value->set_data_type(types::DataType::STRING);
      value->set_string_value(casted_ir.str());
      break;
    }
    case IRNodeType::kFloat: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const FloatIR&>(ir_node);
      value->set_data_type(types::DataType::FLOAT64);
      value->set_float64_value(casted_ir.val());
      break;
    }
    case IRNodeType::kBool: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const BoolIR&>(ir_node);
      value->set_data_type(types::DataType::BOOLEAN);
      value->set_bool_value(casted_ir.val());
      break;
    }
    case IRNodeType::kTime: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const TimeIR&>(ir_node);
      value->set_data_type(types::DataType::TIME64NS);
      value->set_time64_ns_value(static_cast<::google::protobuf::int64>(casted_ir.val()));
      break;
    }
    case IRNodeType::kMetadataLiteral: {
      // MetadataLiteral is just a container.
      auto casted_ir = static_cast<const MetadataLiteralIR&>(ir_node);
      PL_RETURN_IF_ERROR(EvaluateExpression(expr, *casted_ir.literal()));
      break;
    }
    default: {
      return ir_node.CreateIRNodeError("Didn't expect $0 in expression evaluator",
                                       ir_node.type_string());
    }
  }
  return Status::OK();
}

Status MapIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_map_op();
  op->set_op_type(planpb::MAP_OPERATOR);

  for (const auto& col_expr : col_exprs_) {
    auto expr = pb->add_expressions();
    PL_RETURN_IF_ERROR(EvaluateExpression(expr, *col_expr.node));
    pb->add_column_names(col_expr.name);
  }

  return Status::OK();
}

Status FilterIR::Init(OperatorIR* parent, ExpressionIR* expr) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  return SetFilterExpr(expr);
}

Status FilterIR::SetFilterExpr(ExpressionIR* expr) {
  if (filter_expr_) {
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(this, filter_expr_));
  }
  filter_expr_ = expr;
  return graph_ptr()->AddEdge(this, filter_expr_);
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
  PL_RETURN_IF_ERROR(EvaluateExpression(expr, *filter_expr_));
  return Status::OK();
}

Status LimitIR::Init(OperatorIR* parent, int64_t limit_value) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  SetLimitValue(limit_value);
  return Status::OK();
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
  groups_ = groups;
  for (auto g : groups_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, g));
  }
  return Status::OK();
}

Status BlockingAggIR::SetAggExprs(const ColExpressionVector& agg_expr) {
  aggregate_expressions_ = agg_expr;
  for (const auto& expr : aggregate_expressions_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, expr.node));
  }
  return Status::OK();
}

Status GroupByIR::Init(OperatorIR* parent, const std::vector<ColumnIR*>& groups) {
  PL_RETURN_IF_ERROR(AddParent(parent));
  return SetGroups(groups);
}

Status GroupByIR::SetGroups(const std::vector<ColumnIR*>& groups) {
  groups_ = groups;
  for (auto g : groups_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, g));
  }
  return Status::OK();
}

StatusOr<IRNode*> GroupByIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(GroupByIR * group_by, graph->MakeNode<GroupByIR>(id()));
  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : groups_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_column, column->DeepCloneInto(graph));
    DCHECK(Match(new_column, ColumnNode()));
    new_groups.push_back(static_cast<ColumnIR*>(new_column));
  }
  PL_RETURN_IF_ERROR(group_by->SetGroups(new_groups));
  return group_by;
}

Status BlockingAggIR::EvaluateAggregateExpression(planpb::AggregateExpression* expr,
                                                  const IRNode& ir_node) const {
  DCHECK(ir_node.type() == IRNodeType::kFunc);
  auto casted_ir = static_cast<const FuncIR&>(ir_node);
  expr->set_name(casted_ir.func_name());
  expr->set_id(casted_ir.func_id());
  for (types::DataType dt : casted_ir.args_types()) {
    expr->add_args_data_types(dt);
  }
  for (auto ir_arg : casted_ir.args()) {
    auto arg_pb = expr->add_args();
    switch (ir_arg->type()) {
      case IRNodeType::kMetadata:
      case IRNodeType::kColumn: {
        ColumnIR* col_ir = static_cast<ColumnIR*>(ir_arg);
        auto col = arg_pb->mutable_column();
        PL_ASSIGN_OR_RETURN(int64_t ref_op_id, col_ir->ReferenceID());
        col->set_node(ref_op_id);
        col->set_index(col_ir->col_idx());
        break;
      }
      case IRNodeType::kInt: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<IntIR*>(ir_arg);
        value->set_data_type(types::DataType::INT64);
        value->set_int64_value(casted_ir->val());
        break;
      }
      case IRNodeType::kString: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<StringIR*>(ir_arg);
        value->set_data_type(types::DataType::STRING);
        value->set_string_value(casted_ir->str());
        break;
      }
      case IRNodeType::kFloat: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<FloatIR*>(ir_arg);
        value->set_data_type(types::DataType::FLOAT64);
        value->set_float64_value(casted_ir->val());
        break;
      }
      case IRNodeType::kBool: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<BoolIR*>(ir_arg);
        value->set_data_type(types::DataType::BOOLEAN);
        value->set_bool_value(casted_ir->val());
        break;
      }
      case IRNodeType::kTime: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<const TimeIR&>(ir_node);
        value->set_data_type(types::DataType::TIME64NS);
        value->set_time64_ns_value(static_cast<::google::protobuf::int64>(casted_ir.val()));
        break;
      }
      case IRNodeType::kFunc: {
        return ir_node.CreateIRNodeError("agg expressions cannot be nested", ir_node.type_string());
      }
      default: {
        return ir_node.CreateIRNodeError("Didn't expect node of type $0 in expression evaluator.",
                                         ir_node.type_string());
      }
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
    PL_ASSIGN_OR_RETURN(int64_t ref_op_id, group->ReferenceID());
    group_pb->set_node(ref_op_id);
    group_pb->set_index(group->col_idx());
    pb->add_group_names(group->col_name());
  }

  // TODO(nserrino/philkuz): Add support for streaming aggregates in the compiler.
  pb->set_windowed(false);

  op->set_op_type(planpb::AGGREGATE_OPERATOR);
  return Status::OK();
}

Status ColumnIR::Init(const std::string& col_name, int64_t parent_idx) {
  SetColumnName(col_name);
  SetContainingOperatorParentIdx(parent_idx);
  return Status::OK();
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

StatusOr<OperatorIR*> ColumnIR::ReferencedOperator() const {
  DCHECK(container_op_parent_idx_set_);
  PL_ASSIGN_OR_RETURN(OperatorIR * containing_op, ContainingOperator());
  return containing_op->parents()[container_op_parent_idx_];
}

Status StringIR::Init(std::string str) {
  str_ = str;
  return Status::OK();
}

Status CollectionIR::Init(const std::vector<ExpressionIR*>& children) {
  return SetChildren(children);
}

Status CollectionIR::SetChildren(const std::vector<ExpressionIR*>& children) {
  if (!children_.empty()) {
    return CreateIRNodeError(
        "CollectionIR already has children and likely has been created already.");
  }

  for (auto child : children) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, child));
  }
  children_ = children;
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

Status FuncIR::Init(Op op, const std::vector<ExpressionIR*>& args) {
  op_ = op;
  for (auto a : args) {
    PL_RETURN_IF_ERROR(AddArg(a));
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
  literal_ = literal;
  return graph_ptr()->AddEdge(this, literal);
}

bool MetadataResolverIR::HasMetadataColumn(const std::string& col_name) {
  auto md_map_it = metadata_columns_.find(col_name);
  return md_map_it != metadata_columns_.end();
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

// Clone Functions
StatusOr<std::unique_ptr<IR>> IR::Clone() const {
  auto new_ir = std::make_unique<IR>();
  // Iterate through the children.
  for (int64_t i : dag().TopologicalSort()) {
    IRNode* node = Get(i);
    if (new_ir->HasNode(i) && new_ir->Get(i)->type() == node->type()) {
      continue;
    }
    PL_RETURN_IF_ERROR(node->DeepCloneInto(new_ir.get()));
  }
  // TODO(philkuz) check to make sure these are the same.
  new_ir->dag_ = dag_;
  return std::move(new_ir);
}

Status OperatorIR::CopyParents(OperatorIR* new_op) const {
  DCHECK_EQ(new_op->parents_.size(), 0UL);
  for (const auto& parent : parents()) {
    IRNode* new_parent = new_op->graph_ptr()->Get(parent->id());
    DCHECK(Match(new_parent, Operator()));
    PL_RETURN_IF_ERROR(new_op->AddParent(static_cast<OperatorIR*>(new_parent)));
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

StatusOr<IRNode*> ColumnIR::DeepCloneInto(IR* graph) const {
  PL_ASSIGN_OR_RETURN(IRNode * node, IRNode::DeepCloneInto(graph));
  DCHECK(Match(node, ColumnNode()));
  ColumnIR* column = static_cast<ColumnIR*>(node);
  column->col_name_ = col_name_;
  column->col_name_set_ = col_name_set_;
  column->col_idx_ = col_idx_;
  column->evaluated_data_type_ = evaluated_data_type_;
  column->is_data_type_evaluated_ = is_data_type_evaluated_;
  column->container_op_parent_idx_ = container_op_parent_idx_;
  column->container_op_parent_idx_set_ = container_op_parent_idx_set_;
  return column;
}

StatusOr<IRNode*> ColumnIR::DeepCloneIntoImpl(IR* graph) const {
  return graph->MakeNode<ColumnIR>(id());
}

StatusOr<IRNode*> StringIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(StringIR * string_ir, graph->MakeNode<StringIR>(id()));
  string_ir->str_ = str_;
  return string_ir;
}

StatusOr<IRNode*> CollectionIR::DeepCloneIntoCollection(IR* graph, CollectionIR* collection) const {
  std::vector<ExpressionIR*> new_children;
  for (ExpressionIR* child : collection->children()) {
    PL_ASSIGN_OR_RETURN(IRNode * new_child, child->DeepCloneInto(graph));
    DCHECK(Match(new_child, Expression()));
    new_children.push_back(static_cast<ExpressionIR*>(new_child));
  }
  PL_RETURN_IF_ERROR(collection->SetChildren(new_children));
  return collection;
}

StatusOr<IRNode*> ListIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(CollectionIR * collection, graph->MakeNode<ListIR>(id()));
  return DeepCloneIntoCollection(graph, collection);
}

StatusOr<IRNode*> TupleIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(CollectionIR * collection, graph->MakeNode<TupleIR>(id()));
  return DeepCloneIntoCollection(graph, collection);
}

StatusOr<IRNode*> FuncIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(FuncIR * func, graph->MakeNode<FuncIR>(id()));
  func->func_prefix_ = func_prefix_;
  func->op_ = op_;
  func->func_name_ = func_name_;
  func->args_types_ = args_types_;
  func->func_id_ = func_id_;
  func->evaluated_data_type_ = evaluated_data_type_;
  func->is_data_type_evaluated_ = is_data_type_evaluated_;

  for (ExpressionIR* arg : args_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_arg, arg->DeepCloneInto(graph));
    DCHECK(Match(new_arg, Expression()));
    PL_RETURN_IF_ERROR(func->AddArg(static_cast<ExpressionIR*>(new_arg)));
  }
  return func;
}

StatusOr<IRNode*> FloatIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(FloatIR * float_ir, graph->MakeNode<FloatIR>(id()));
  float_ir->val_ = val_;
  return float_ir;
}

StatusOr<IRNode*> IntIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(IntIR * int_ir, graph->MakeNode<IntIR>(id()));
  int_ir->val_ = val_;
  return int_ir;
}

StatusOr<IRNode*> BoolIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(BoolIR * bool_ir, graph->MakeNode<BoolIR>(id()));
  bool_ir->val_ = val_;
  return bool_ir;
}

StatusOr<IRNode*> TimeIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(TimeIR * time_ir, graph->MakeNode<TimeIR>(id()));
  time_ir->val_ = val_;
  return time_ir;
}

StatusOr<IRNode*> MetadataIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(MetadataIR * metadata, graph->MakeNode<MetadataIR>(id()));
  metadata->metadata_name_ = metadata_name_;
  return metadata;
}

StatusOr<IRNode*> MetadataLiteralIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(MetadataLiteralIR * metadata, graph->MakeNode<MetadataLiteralIR>(id()));
  PL_ASSIGN_OR_RETURN(IRNode * new_literal, literal_->DeepCloneInto(graph));
  DCHECK(Match(new_literal, DataNode())) << new_literal->DebugString();
  PL_RETURN_IF_ERROR(metadata->SetLiteral(static_cast<DataIR*>(new_literal)));
  return metadata;
}

StatusOr<IRNode*> MemorySourceIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(MemorySourceIR * mem, graph->MakeNode<MemorySourceIR>(id()));
  mem->table_name_ = table_name_;
  mem->time_set_ = time_set_;
  mem->time_start_ns_ = time_start_ns_;
  mem->time_stop_ns_ = time_stop_ns_;
  mem->column_names_ = column_names_;
  mem->column_index_map_set_ = column_index_map_set_;
  mem->column_index_map_ = column_index_map_;
  mem->has_time_expressions_ = has_time_expressions_;

  if (has_time_expressions_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_start_expr, start_time_expr_->DeepCloneInto(graph));
    DCHECK(Match(new_start_expr, Expression()));
    PL_ASSIGN_OR_RETURN(IRNode * new_stop_expr, end_time_expr_->DeepCloneInto(graph));
    DCHECK(Match(new_stop_expr, Expression()));
    PL_RETURN_IF_ERROR(mem->SetTimeExpressions(static_cast<ExpressionIR*>(new_start_expr),
                                               static_cast<ExpressionIR*>(new_stop_expr)));
  }
  return mem;
}

StatusOr<IRNode*> MemorySinkIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(MemorySinkIR * mem, graph->MakeNode<MemorySinkIR>(id()));
  mem->name_ = name_;
  mem->name_set_ = name_set_;
  return mem;
}

StatusOr<IRNode*> RangeIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(RangeIR * range, graph->MakeNode<RangeIR>(id()));
  PL_ASSIGN_OR_RETURN(range->start_repr_, start_repr_->DeepCloneInto(graph));
  PL_ASSIGN_OR_RETURN(range->stop_repr_, stop_repr_->DeepCloneInto(graph));
  return range;
}

StatusOr<IRNode*> MetadataResolverIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(MetadataResolverIR * metadata_resolver,
                      graph->MakeNode<MetadataResolverIR>(id()));
  metadata_resolver->metadata_columns_ = metadata_columns_;
  return metadata_resolver;
}

StatusOr<IRNode*> OperatorIR::DeepCloneInto(IR* graph) const {
  PL_ASSIGN_OR_RETURN(IRNode * node, IRNode::DeepCloneInto(graph));
  DCHECK(node->IsOperator());
  OperatorIR* new_op = static_cast<OperatorIR*>(node);
  PL_RETURN_IF_ERROR(CopyParents(new_op));

  new_op->is_source_ = is_source_;
  new_op->relation_ = relation_;
  new_op->relation_init_ = relation_init_;
  return new_op;
}

StatusOr<IRNode*> MapIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(MapIR * map, graph->MakeNode<MapIR>(id()));
  ColExpressionVector new_col_exprs;
  for (const ColumnExpression& col_expr : col_exprs_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_node, col_expr.node->DeepCloneInto(graph));
    DCHECK(Match(new_node, Expression()));
    new_col_exprs.push_back({col_expr.name, static_cast<ExpressionIR*>(new_node)});
  }

  PL_RETURN_IF_ERROR(map->SetColExprs(new_col_exprs));
  map->keep_input_columns_ = keep_input_columns_;
  return map;
}

StatusOr<IRNode*> DropIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(DropIR * drop, graph->MakeNode<DropIR>(id()));
  drop->col_names_ = col_names_;
  return drop;
}

StatusOr<IRNode*> BlockingAggIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(BlockingAggIR * blocking_agg, graph->MakeNode<BlockingAggIR>(id()));
  ColExpressionVector new_agg_exprs;
  for (const ColumnExpression& col_expr : aggregate_expressions_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_node, col_expr.node->DeepCloneInto(graph));
    DCHECK(Match(new_node, Expression()));
    new_agg_exprs.push_back({col_expr.name, static_cast<ExpressionIR*>(new_node)});
  }

  std::vector<ColumnIR*> new_groups;
  for (const ColumnIR* column : groups_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_column, column->DeepCloneInto(graph));
    DCHECK(Match(new_column, ColumnNode()));
    new_groups.push_back(static_cast<ColumnIR*>(new_column));
  }

  PL_RETURN_IF_ERROR(blocking_agg->SetAggExprs(new_agg_exprs));
  PL_RETURN_IF_ERROR(blocking_agg->SetGroups(new_groups));

  return blocking_agg;
}

StatusOr<IRNode*> FilterIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(FilterIR * filter, graph->MakeNode<FilterIR>(id()));
  PL_ASSIGN_OR_RETURN(IRNode * new_node, filter_expr_->DeepCloneInto(graph));
  DCHECK(Match(new_node, Expression()));
  PL_RETURN_IF_ERROR(filter->SetFilterExpr(static_cast<ExpressionIR*>(new_node)));
  return filter;
}

StatusOr<IRNode*> LimitIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(LimitIR * limit, graph->MakeNode<LimitIR>(id()));
  limit->limit_value_ = limit_value_;
  limit->limit_value_set_ = limit_value_set_;
  return limit;
}

StatusOr<IRNode*> GRPCSinkIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(GRPCSinkIR * grpc_sink, graph->MakeNode<GRPCSinkIR>(id()));
  grpc_sink->destination_id_ = destination_id_;
  grpc_sink->physical_id_ = physical_id_;
  grpc_sink->destination_address_ = destination_address_;
  return grpc_sink;
}

StatusOr<IRNode*> GRPCSourceGroupIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(GRPCSourceGroupIR * grpc_source_group,
                      graph->MakeNode<GRPCSourceGroupIR>(id()));
  grpc_source_group->source_id_ = source_id_;
  grpc_source_group->remote_string_ids_ = remote_string_ids_;
  grpc_source_group->grpc_address_ = grpc_address_;
  return grpc_source_group;
}

StatusOr<IRNode*> GRPCSourceIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(GRPCSourceIR * grpc_source, graph->MakeNode<GRPCSourceIR>(id()));
  grpc_source->remote_source_id_ = remote_source_id_;
  return grpc_source;
}

Status GRPCSourceGroupIR::ToProto(planpb::Operator* op) const {
  // Note this is more for testing.
  auto pb = op->mutable_grpc_source_op();
  op->set_op_type(planpb::GRPC_SOURCE_OPERATOR);

  pb->set_source_id(absl::StrCat(source_id_));
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
  pb->set_destination_id(DistributedDestinationID());
  return Status::OK();
}

Status GRPCSourceIR::ToProto(planpb::Operator* op) const {
  auto pb = op->mutable_grpc_source_op();
  op->set_op_type(planpb::GRPC_SOURCE_OPERATOR);

  pb->set_source_id(remote_source_id_);
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

Status IR::Prune(const std::unordered_set<int64_t>& ids_to_prune) {
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

Status GRPCSourceGroupIR::AddGRPCSink(GRPCSinkIR* sink_op) {
  if (sink_op->destination_id() != source_id_) {
    return DExitOrIRNodeError("Source id $0 and destination id $1 aren't equal.",
                              sink_op->destination_id(), source_id_);
  }
  if (!GRPCAddressSet()) {
    return DExitOrIRNodeError("$0 doesn't have a physical agent associated with it.",
                              DebugString());
  }
  if (!sink_op->DistributedIDSet()) {
    return DExitOrIRNodeError("$0 doesn't have a physical agent associated with it.",
                              sink_op->DebugString());
  }
  remote_string_ids_.push_back(sink_op->DistributedDestinationID());
  sink_op->SetDestinationAddress(grpc_address_);
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
    for (const auto col_idx : column_mapping.input_column_map) {
      pb_column_mapping->add_column_indexes(col_idx);
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

StatusOr<IRNode*> UnionIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(UnionIR * union_node, graph->MakeNode<UnionIR>(id()));
  union_node->column_mappings_ = column_mappings_;
  return union_node;
}

Status UnionIR::AddColumnMapping(const std::vector<int64_t>& column_mapping) {
  DCHECK(IsRelationInit()) << "Relation must be initialized before running this.";
  if (column_mapping.size() != relation().NumColumns()) {
    return DExitOrIRNodeError("Expected colums mapping to match the relation size. $0 vs $1",
                              column_mapping.size(), relation().NumColumns());
  }
  column_mappings_.push_back({column_mapping});
  return Status::OK();
}

Status UnionIR::SetRelationFromParents() {
  DCHECK_NE(parents().size(), 0UL);

  std::vector<Relation> relations;
  OperatorIR* base_parent = parents()[0];
  Relation base_relation = base_parent->relation();
  PL_RETURN_IF_ERROR(SetRelation(base_relation));

  for (size_t i = 0; i < parents().size(); ++i) {
    OperatorIR* cur_parent = parents()[i];
    Relation cur_relation = cur_parent->relation();
    std::string err_msg = absl::Substitute(
        "Table schema disagreement between parent ops $0 and $1 of $2. $0: $3 vs $1: $4. $5",
        base_parent->DebugString(), cur_parent->DebugString(), DebugString(),
        base_relation.DebugString(), cur_relation.DebugString(), "$0");
    if (cur_relation.NumColumns() != base_relation.NumColumns()) {
      return CreateIRNodeError(err_msg, "Column count wrong.");
    }
    std::vector<int64_t> column_mapping;
    for (int64_t col_idx = 0; col_idx < static_cast<int64_t>(base_relation.NumColumns());
         ++col_idx) {
      std::string base_relation_name = base_relation.GetColumnName(col_idx);
      types::DataType base_relation_type = base_relation.GetColumnType(col_idx);
      if (!cur_relation.HasColumn(base_relation_name) ||
          cur_relation.GetColumnType(base_relation_name) != base_relation_type) {
        return CreateIRNodeError(
            err_msg, absl::Substitute("Missing or wrong type for $0.", base_relation_name));
      }
      column_mapping.push_back(cur_relation.GetColumnIndex(base_relation_name));
    }
    PL_RETURN_IF_ERROR(AddColumnMapping(column_mapping));
  }
  return Status::OK();
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
    eq_condition->set_left_column_index(left_on_columns_[i]->col_idx());
    eq_condition->set_right_column_index(right_on_columns_[i]->col_idx());
  }

  for (ColumnIR* col : output_columns_) {
    auto* parent_col = pb->add_output_columns();
    int64_t parent_idx = col->container_op_parent_idx();
    DCHECK_LT(parent_idx, 2);
    parent_col->set_parent_index(col->container_op_parent_idx());
    DCHECK(col->IsDataTypeEvaluated()) << "Column not evaluated";
    parent_col->set_column_index(col->col_idx());
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

  for (auto* p : parents) {
    PL_RETURN_IF_ERROR(AddParent(p));
  }

  PL_RETURN_IF_ERROR(SetJoinColumns(left_on_cols, right_on_cols));

  suffix_strs_ = suffix_strs;
  return SetJoinType(how_type);
}

Status JoinIR::SetJoinColumns(const std::vector<ColumnIR*>& left_columns,
                              const std::vector<ColumnIR*>& right_columns) {
  left_on_columns_ = left_columns;
  for (auto g : left_on_columns_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, g));
  }

  right_on_columns_ = right_columns;
  for (auto g : right_on_columns_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, g));
  }
  return Status::OK();
}

StatusOr<IRNode*> JoinIR::DeepCloneIntoImpl(IR* graph) const {
  PL_ASSIGN_OR_RETURN(JoinIR * join_node, graph->MakeNode<JoinIR>(id()));
  join_node->join_type_ = join_type_;

  std::vector<ColumnIR*> new_output_columns;
  for (ColumnIR* col_expr : output_columns_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_node, col_expr->DeepCloneInto(graph));
    DCHECK(Match(new_node, ColumnNode()));
    new_output_columns.push_back(static_cast<ColumnIR*>(new_node));
  }
  PL_RETURN_IF_ERROR(join_node->SetOutputColumns(column_names_, new_output_columns));

  std::vector<ColumnIR*> new_left_columns;
  for (ColumnIR* col_expr : left_on_columns_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_node, col_expr->DeepCloneInto(graph));
    DCHECK(Match(new_node, ColumnNode()));
    new_left_columns.push_back(static_cast<ColumnIR*>(new_node));
  }

  std::vector<ColumnIR*> new_right_columns;
  for (ColumnIR* col_expr : right_on_columns_) {
    PL_ASSIGN_OR_RETURN(IRNode * new_node, col_expr->DeepCloneInto(graph));
    DCHECK(Match(new_node, ColumnNode()));
    new_right_columns.push_back(static_cast<ColumnIR*>(new_node));
  }

  PL_RETURN_IF_ERROR(join_node->SetJoinColumns(new_left_columns, new_right_columns));
  join_node->suffix_strs_ = suffix_strs_;

  return join_node;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
