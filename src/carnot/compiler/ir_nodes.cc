#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

Status IR::AddEdge(int64_t from_node, int64_t to_node) {
  dag_.AddEdge(from_node, to_node);
  return Status::OK();
}

Status IR::AddEdge(IRNode* from_node, IRNode* to_node) {
  return AddEdge(from_node->id(), to_node->id());
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

Status IR::DeleteNode(int64_t node) {
  if (!dag_.HasNode(node)) {
    return error::InvalidArgument("No node $0 exists in graph.", node);
  }
  dag_.DeleteNode(node);
  return Status::OK();
}

std::string IR::DebugString() {
  std::string debug_string = dag().DebugString() + "\n";
  for (auto const& a : id_node_map_) {
    debug_string += a.second->DebugString() + "\n";
  }
  return debug_string;
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
      PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(new_parent->id(), id()));
      PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(old_parent->id(), id()));
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

Status OperatorIR::ArgMapContainsKeys(const ArgMap& args) {
  std::vector<std::string> missing_keys;
  for (const auto& arg : ArgKeys()) {
    if (args.find(arg) == args.end()) {
      missing_keys.push_back(arg);
    }
  }
  if (missing_keys.size() != 0) {
    return CreateIRNodeError("Missing args [$0] in call. ", absl::StrJoin(missing_keys, ","));
  }
  return Status::OK();
}

Status OperatorIR::Init(OperatorIR* parent, const ArgMap& args, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(ArgMapContainsKeys(args));
  if (parent != nullptr) {
    PL_RETURN_IF_ERROR(AddParent(parent));
  }
  PL_RETURN_IF_ERROR(InitImpl(args));
  return Status::OK();
}

bool MemorySourceIR::HasLogicalRepr() const { return true; }

Status MemorySourceIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::MemorySourceOperator();
  pb->set_name(table_name_);

  if (!columns_set()) {
    return error::InvalidArgument("MemorySource columns are not set.");
  }

  for (const auto& col : columns_) {
    pb->add_column_idxs(col->col_idx());
    pb->add_column_names(col->col_name());
    pb->add_column_types(col->EvaluatedDataType());
  }

  if (IsTimeSet()) {
    auto start_time = new ::google::protobuf::Int64Value();
    start_time->set_value(time_start_ns_);
    pb->set_allocated_start_time(start_time);
    auto stop_time = new ::google::protobuf::Int64Value();
    stop_time->set_value(time_stop_ns_);
    pb->set_allocated_stop_time(stop_time);
  }

  op->set_op_type(planpb::MEMORY_SOURCE_OPERATOR);
  op->set_allocated_mem_source_op(pb);
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

bool MemorySinkIR::HasLogicalRepr() const { return true; }

Status MemorySinkIR::InitImpl(const ArgMap& args) {
  DCHECK(args.find("name") != args.end());
  IRNode* name_node = args.find("name")->second;
  if (name_node->type() != IRNodeType::kString) {
    return name_node->CreateIRNodeError("Expected string. Got $0", name_node->type_string());
  }
  name_ = static_cast<StringIR*>(name_node)->str();
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, name_node));
  name_set_ = true;
  return Status::OK();
}

Status MemorySourceIR::InitImpl(const ArgMap& args) {
  DCHECK(args.find("table") != args.end());
  DCHECK(args.find("select") != args.end());

  IRNode* table_node = args.find("table")->second;
  if (table_node->type() != IRNodeType::kString) {
    return CreateIRNodeError("Expected table argument to be a string, not a $0",
                             table_node->type_string());
  }
  table_name_ = static_cast<StringIR*>(table_node)->str();
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, table_node));

  IRNode* select_node = args.find("select")->second;
  if (select_node == nullptr) {
    select_ = nullptr;
    return Status::OK();
  }
  if (select_node->type() != IRNodeType::kList) {
    return CreateIRNodeError("Expected select argument to be a list, not a $0",
                             table_node->type_string());
  }
  select_ = static_cast<ListIR*>(select_node);

  return graph_ptr()->AddEdge(this, select_);
}

// TODO(philkuz) impl
Status RangeIR::InitImpl(const ArgMap& args) {
  PL_UNUSED(args);
  return Status::OK();
}

Status MemorySinkIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::MemorySinkOperator();
  pb->set_name(name_);

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  op->set_op_type(planpb::MEMORY_SINK_OPERATOR);
  op->set_allocated_mem_sink_op(pb);
  return Status::OK();
}

Status RangeIR::Init(OperatorIR* parent_node, IRNode* start_repr, IRNode* stop_repr,
                     const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
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

bool RangeIR::HasLogicalRepr() const { return false; }

Status RangeIR::ToProto(planpb::Operator*) const {
  return error::InvalidArgument("RangeIR has no protobuf representation.");
}

Status MapIR::InitImpl(const ArgMap& args) {
  DCHECK(args.find("fn") != args.end());
  IRNode* lambda_func_node = args.find("fn")->second;
  if (lambda_func_node->type() != IRNodeType::kLambda) {
    return CreateIRNodeError("Expected 'fn' argument of Agg to be a lambda, got '$0'",
                             lambda_func_node->type_string());
  }
  return SetupMapExpressions(static_cast<LambdaIR*>(lambda_func_node));
}

Status MapIR::SetupMapExpressions(LambdaIR* map_func) {
  if (!map_func->HasDictBody()) {
    return map_func->CreateIRNodeError("Expected lambda func to have dictionary body.");
  }
  col_exprs_ = map_func->col_exprs();
  for (const ColumnExpression& mapped_expression : col_exprs_) {
    ExpressionIR* expr = mapped_expression.node;
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(map_func->id(), expr->id()));
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, expr));
  }
  return graph_ptr()->DeleteNode(map_func->id());
}

bool MapIR::HasLogicalRepr() const { return true; }

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
      return error::InvalidArgument("Didn't expect $0 in expression evaluator.",
                                    ir_node.type_string());
    }
  }
  return Status::OK();
}

Status MapIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::MapOperator();

  for (const auto& col_expr : col_exprs_) {
    auto expr = pb->add_expressions();
    PL_RETURN_IF_ERROR(EvaluateExpression(expr, *col_expr.node));
    pb->add_column_names(col_expr.name);
  }

  op->set_op_type(planpb::MAP_OPERATOR);
  op->set_allocated_map_op(pb);
  return Status::OK();
}

Status FilterIR::InitImpl(const ArgMap& args) {
  DCHECK(args.find("fn") != args.end());
  IRNode* filter_func_node = args.find("fn")->second;
  if (filter_func_node->type() != IRNodeType::kLambda) {
    return CreateIRNodeError("Expected 'fn' argument of Filter to be a 'lambda', got '$0'",
                             filter_func_node->type_string());
  }
  LambdaIR* filter_func = static_cast<LambdaIR*>(filter_func_node);

  if (filter_func->HasDictBody()) {
    return CreateIRNodeError(
        "Expected lambda of the Filter to contain a single expression, not a dictionary.");
  }

  PL_ASSIGN_OR_RETURN(filter_expr_, filter_func->GetDefaultExpr());
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, filter_expr_));

  // Clean up the lambda.
  PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(filter_func->id(), filter_expr_->id()));
  return graph_ptr()->DeleteNode(filter_func->id());
}

bool FilterIR::HasLogicalRepr() const { return true; }

Status FilterIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::FilterOperator();
  DCHECK_EQ(parents().size(), 1UL);

  for (size_t i = 0; i < relation().NumColumns(); ++i) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parents()[0]->id());
    col_pb->set_index(i);
  }

  auto expr = new planpb::ScalarExpression();
  PL_RETURN_IF_ERROR(EvaluateExpression(expr, *filter_expr_));
  pb->set_allocated_expression(expr);

  op->set_op_type(planpb::FILTER_OPERATOR);
  op->set_allocated_filter_op(pb);
  return Status::OK();
}

Status LimitIR::InitImpl(const ArgMap& args) {
  DCHECK(args.find("rows") != args.end());
  IRNode* limit_node = args.find("rows")->second;
  if (limit_node->type() != IRNodeType::kInt) {
    return CreateIRNodeError("Expected 'int', got $0", limit_node->type_string());
  }

  SetLimitValue(static_cast<IntIR*>(limit_node)->val());
  return graph_ptr()->AddEdge(this, limit_node);
}

bool LimitIR::HasLogicalRepr() const { return true; }

Status LimitIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::LimitOperator();
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

  op->set_op_type(planpb::LIMIT_OPERATOR);
  op->set_allocated_limit_op(pb);
  return Status::OK();
}

// TODO(philkuz) fix up the initimpl to make this less hardcoded
Status BlockingAggIR::InitImpl(const ArgMap& args) {
  IRNode* by_func = args.find("by")->second;
  IRNode* agg_func = args.find("fn")->second;
  if (agg_func->type() != IRNodeType::kLambda) {
    return CreateIRNodeError("Expected 'agg' argument of Agg to be 'Lambda', got '$0'",
                             agg_func->type_string());
  }

  // By_func is either nullptr or a lambda.
  if (by_func != nullptr && by_func->type() != IRNodeType::kLambda) {
    return CreateIRNodeError("Expected 'by' argument of Agg to be 'Lambda', got '$0'",
                             by_func->type_string());
  } else if (by_func != nullptr) {
    PL_RETURN_IF_ERROR(SetupGroupBy(static_cast<LambdaIR*>(by_func)));
  }

  PL_RETURN_IF_ERROR(SetupAggFunctions(static_cast<LambdaIR*>(agg_func)));

  return Status::OK();
}

Status BlockingAggIR::SetupGroupBy(LambdaIR* by_lambda) {
  // Make sure default expr
  // Convert to list of groups.
  if (by_lambda->HasDictBody()) {
    return CreateIRNodeError(
        "Expected `by` argument lambda body of Agg to be a single column or a list of columns. A "
        "dictionary is not allowed.");
  }
  PL_ASSIGN_OR_RETURN(ExpressionIR * by_expr, by_lambda->GetDefaultExpr());
  if (by_expr->type() == IRNodeType::kList) {
    for (ExpressionIR* child : static_cast<ListIR*>(by_expr)->children()) {
      if (!child->IsColumn()) {
        return child->CreateIRNodeError(
            "Expected `by` argument lambda body of Agg to be a single column or a list of "
            "columns. "
            "A list containing a '$0' is not allowed.",
            child->type_string());
      }
      groups_.push_back(static_cast<ColumnIR*>(child));
      // Delete the list->column edge.
      PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(by_expr->id(), child->id()));
    }
    // Delete the lambda edge.
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(by_lambda->id(), by_expr->id()));
    // Delete list.
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteNode(by_expr->id()));
  } else if (by_expr->IsColumn()) {
    groups_.push_back(static_cast<ColumnIR*>(by_expr));
    // Delete the lambda edge.
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(by_lambda->id(), by_expr->id()));
  } else {
    return CreateIRNodeError(
        "Expected `by` argument lambda body of Agg to be a single column or a list of columns, not "
        "a '$0'.",
        by_expr->type_string());
  }
  // Clean up the pointer to parent
  for (ColumnIR* g : groups_) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(id(), g->id()));
  }
  return Status::OK();
}

Status BlockingAggIR::SetupAggFunctions(LambdaIR* agg_func) {
  // Make a new relation with each of the expression key, type pairs.
  if (!agg_func->HasDictBody()) {
    return agg_func->CreateIRNodeError(
        "Expected `fn` arg's lambda body to be a dictionary mapping string column names to "
        "aggregate expression.");
  }

  ColExpressionVector col_exprs = agg_func->col_exprs();
  for (const auto& expr_struct : col_exprs) {
    ExpressionIR* expr = expr_struct.node;
    // check that the expression type is a function and that it only has leaf nodes as children.
    if (expr->type() != IRNodeType::kFunc) {
      return expr->CreateIRNodeError(
          "Expected aggregate expression '$0' to be an aggregate function of the format "
          "\"<fn-name>(r.column_name)\". $0 not allowed.",
          expr->type_string());
    }
    auto func = static_cast<FuncIR*>(expr);
    for (const auto& fn_child : func->args()) {
      if (fn_child->type() == IRNodeType::kFunc) {
        return fn_child->CreateIRNodeError("Nested aggregate expressions not allowed.");
      }
    }
    PL_RETURN_IF_ERROR(graph_ptr()->DeleteEdge(agg_func->id(), expr->id()));
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(id(), expr->id()));
  }
  aggregate_expressions_ = std::move(col_exprs);
  // Remove the node.
  return graph_ptr()->DeleteNode(agg_func->id());
}

bool BlockingAggIR::HasLogicalRepr() const { return true; }

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
      default: {
        return error::InvalidArgument("Didn't expect node of type $0 in expression evaluator.",
                                      ir_node.type_string());
      }
    }
  }
  return Status::OK();
}

Status BlockingAggIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::AggregateOperator();

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
  op->set_allocated_agg_op(pb);
  return Status::OK();
}

bool ColumnIR::HasLogicalRepr() const { return false; }
Status ColumnIR::Init(const std::string& col_name, int64_t parent_idx,
                      const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  SetColumnName(col_name);
  SetContainingOperatoreratorParentIdx(parent_idx);
  return Status::OK();
}

std::string ColumnIR::DebugString() const {
  return absl::Substitute("$0(id=$1, name=$2)", type_string(), id(), col_name());
}

void ColumnIR::SetContainingOperatoreratorParentIdx(int64_t container_op_parent_idx) {
  DCHECK_GE(container_op_parent_idx, 0);
  container_op_parent_idx_ = container_op_parent_idx;
  container_op_parent_idx_set_ = true;
}

StatusOr<OperatorIR*> ColumnIR::ReferencedOperator() const {
  DCHECK(container_op_parent_idx_set_);
  PL_ASSIGN_OR_RETURN(OperatorIR * containing_op, ContainingOperator());
  return containing_op->parents()[container_op_parent_idx_];
}

bool StringIR::HasLogicalRepr() const { return false; }
Status StringIR::Init(std::string str, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  str_ = str;
  return Status::OK();
}

bool ListIR::HasLogicalRepr() const { return false; }
Status ListIR::Init(const pypa::AstPtr& ast_node, std::vector<ExpressionIR*> children) {
  if (!children_.empty()) {
    return error::AlreadyExists("ListIR already has children and likely has been created already.");
  }
  SetLineCol(ast_node);
  for (auto child : children) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, child));
  }
  children_ = children;
  return Status::OK();
}

bool LambdaIR::HasLogicalRepr() const { return false; }
bool LambdaIR::HasDictBody() const { return has_dict_body_; }

Status LambdaIR::Init(std::unordered_set<std::string> expected_column_names,
                      const ColExpressionVector& col_exprs, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  expected_column_names_ = expected_column_names;
  col_exprs_ = col_exprs;
  for (const ColumnExpression& col_expr : col_exprs) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, col_expr.node));
  }
  has_dict_body_ = true;
  return Status::OK();
}

Status LambdaIR::Init(std::unordered_set<std::string> expected_column_names, ExpressionIR* node,
                      const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  expected_column_names_ = expected_column_names;
  col_exprs_.push_back(ColumnExpression{default_key, node});
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, node));
  has_dict_body_ = false;
  return Status::OK();
}

StatusOr<ExpressionIR*> LambdaIR::GetDefaultExpr() {
  if (HasDictBody()) {
    return error::InvalidArgument(
        "Couldn't return the default expression, Lambda initialized as dict.");
  }
  for (const auto& col_expr : col_exprs_) {
    if (col_expr.name == default_key) {
      return col_expr.node;
    }
  }
  return error::InvalidArgument(
      "Couldn't return the default expression, no default expression in column expression "
      "vector.");
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
bool FuncIR::HasLogicalRepr() const { return false; }
Status FuncIR::Init(Op op, std::string func_prefix, const std::vector<ExpressionIR*>& args,
                    bool compile_time, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  op_ = op;
  func_prefix_ = func_prefix;
  args_ = args;
  is_compile_time_ = compile_time;
  for (auto a : args_) {
    if (a == nullptr) {
      return error::Internal("Argument for FuncIR is null.");
    }
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, a));
  }
  return Status::OK();
}

/* Float IR */
bool FloatIR::HasLogicalRepr() const { return false; }
Status FloatIR::Init(double val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
/* Int IR */
bool IntIR::HasLogicalRepr() const { return false; }
Status IntIR::Init(int64_t val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
/* Bool IR */
bool BoolIR::HasLogicalRepr() const { return false; }
Status BoolIR::Init(bool val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}

/* Time IR */
bool TimeIR::HasLogicalRepr() const { return false; }
Status TimeIR::Init(int64_t val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}

/* Metadata IR */
Status MetadataIR::Init(const std::string& metadata_str, int64_t parent_op_idx,
                        const pypa::AstPtr& ast_node) {
  // Note, metadata_str is a temporary name. It is updated in ResolveMetadataColumn.
  PL_RETURN_IF_ERROR(ColumnIR::Init(metadata_str, parent_op_idx, ast_node));
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
Status MetadataLiteralIR::Init(DataIR* literal, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, literal));
  literal_ = literal;
  literal_type_ = literal->type();
  return Status::OK();
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

Status MetadataResolverIR::InitImpl(const ArgMap&) { return Status::OK(); }

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
