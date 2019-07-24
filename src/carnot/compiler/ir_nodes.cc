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
    debug_string += a.second->DebugString(0) + "\n";
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
// TODO(philkuz) refactor this into AddParent, add expected_num_parents for each operator, and add
// RemoveParent().
Status OperatorIR::SetParent(IRNode* node) {
  if (!node->IsOp()) {
    return error::InvalidArgument("Expected Op, got $0 instead", node->type_string());
  }
  parent_ = static_cast<OperatorIR*>(node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent_, this));
  return Status::OK();
}

Status OperatorIR::RemoveParent(OperatorIR* parent) {
  return graph_ptr()->DeleteEdge(parent->id(), id());
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

Status OperatorIR::Init(IRNode* parent, const ArgMap& args, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(ArgMapContainsKeys(args));
  if (parent != nullptr) {
    PL_RETURN_IF_ERROR(SetParent(parent));
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
std::string MemorySourceIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> property_map = {{"From", table_name_}};
  if (!select_all()) {
    CHECK(select_ != nullptr);
    property_map["Select"] = select_->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::Substitute("$0:MemorySourceIR", id()), property_map);
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

  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, select_));
  return Status::OK();
}

// TODO(philkuz) impl
Status RangeIR::InitImpl(const ArgMap& args) {
  PL_UNUSED(args);
  return Status::OK();
}

std::string MemorySinkIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::Substitute("$0:MemorySinkIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)}});
}

Status MemorySinkIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::MemorySinkOperator();
  pb->set_name(name_);

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  op->set_op_type(planpb::MEMORY_SINK_OPERATOR);
  op->set_allocated_mem_sink_op(pb);
  return Status::OK();
}

Status RangeIR::Init(IRNode* parent_node, IRNode* start_repr, IRNode* stop_repr,
                     const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(SetParent(parent_node));
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
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, stop_repr_));
  return Status::OK();
}

bool RangeIR::HasLogicalRepr() const { return false; }

std::string RangeIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::Substitute("$0:RangeIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Start", start_repr_->DebugString(depth + 1)},
                         {"Stop", stop_repr_->DebugString(depth + 1)}});
}

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
  lambda_func_ = static_cast<LambdaIR*>(lambda_func_node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, lambda_func_));
  return Status::OK();
}

bool MapIR::HasLogicalRepr() const { return true; }

std::string MapIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::Substitute("$0:MapIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Lambda", lambda_func_->DebugString(depth + 1)}});
}

Status OperatorIR::EvaluateExpression(planpb::ScalarExpression* expr, const IRNode& ir_node) const {
  switch (ir_node.type()) {
    case IRNodeType::kMetadata:
    case IRNodeType::kColumn: {
      auto col = expr->mutable_column();
      col->set_node(parent()->id());
      col->set_index(static_cast<const ColumnIR&>(ir_node).col_idx());
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
    default: {
      return error::InvalidArgument("Didn't expect node of type $0 in expression evaluator.",
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
  filter_func_ = static_cast<LambdaIR*>(filter_func_node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, filter_func_));
  return Status::OK();
}

bool FilterIR::HasLogicalRepr() const { return true; }

std::string FilterIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::Substitute("$0:FilterIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Filter", filter_func_->DebugString(depth + 1)}});
}

Status FilterIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::FilterOperator();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parent()->id());
    col_pb->set_index(i);
  }

  auto expr = new planpb::ScalarExpression();
  PL_ASSIGN_OR_RETURN(auto lambda_expr, static_cast<LambdaIR*>(filter_func_)->GetDefaultExpr());
  PL_RETURN_IF_ERROR(EvaluateExpression(expr, *lambda_expr));
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
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, limit_node));
  return Status::OK();
}

bool LimitIR::HasLogicalRepr() const { return true; }

std::string LimitIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::Substitute("$0:LimitIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Limit", absl::Substitute("$0", limit_value_)}});
}

Status LimitIR::ToProto(planpb::Operator* op) const {
  auto pb = new planpb::LimitOperator();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    planpb::Column* col_pb = pb->add_columns();
    col_pb->set_node(parent()->id());
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

  // If by_func_ is not a null pointer, then update the graph with it. Otherwise, continue
  // onwards.
  if (by_func == nullptr) {
    by_func_ = nullptr;
  } else if (by_func->type() == IRNodeType::kLambda) {
    PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, by_func));
    by_func_ = static_cast<LambdaIR*>(by_func);
  } else {
    return CreateIRNodeError("Expected 'by' argument of Agg to be 'Lambda', got '$0'",
                             by_func->type_string());
  }

  agg_func_ = static_cast<LambdaIR*>(agg_func);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, agg_func_));

  return Status();
}

bool BlockingAggIR::HasLogicalRepr() const { return true; }

std::string BlockingAggIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> property_map = {{"Parent", parent()->DebugString(depth + 1)},
                                                     {"AggFn", agg_func_->DebugString(depth + 1)}};
  if (by_func_ != nullptr) {
    property_map["ByFn"] = by_func_->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::Substitute("$0:BlockingAggIR", id()), property_map);
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
        auto col = arg_pb->mutable_column();
        col->set_node(parent()->id());
        col->set_index(static_cast<ColumnIR*>(ir_arg)->col_idx());
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

  for (const auto& agg_expr : agg_val_vector_) {
    auto expr = pb->add_values();
    PL_RETURN_IF_ERROR(EvaluateAggregateExpression(expr, *agg_expr.node));
    pb->add_value_names(agg_expr.name);
  }

  if (by_func_ != nullptr) {
    for (const auto& group : groups_) {
      auto group_pb = pb->add_groups();
      group_pb->set_node(parent()->id());
      group_pb->set_index(group->col_idx());
      pb->add_group_names(group->col_name());
    }
  }

  // TODO(nserrino/philkuz): Add support for streaming aggregates in the compiler.
  pb->set_windowed(false);

  op->set_op_type(planpb::AGGREGATE_OPERATOR);
  op->set_allocated_agg_op(pb);
  return Status::OK();
}

bool ColumnIR::HasLogicalRepr() const { return false; }
Status ColumnIR::Init(const std::string& col_name, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  col_name_ = col_name;
  return Status::OK();
}

std::string ColumnIR::DebugString(int64_t depth) const {
  return absl::Substitute("$0$1:$2\t-\t$3", std::string(depth, '\t'), id(), "Column", col_name());
}

bool StringIR::HasLogicalRepr() const { return false; }
Status StringIR::Init(std::string str, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  str_ = str;
  return Status::OK();
}

std::string StringIR::DebugString(int64_t depth) const {
  return absl::Substitute("$0$1:$2\t-\t$3", std::string(depth, '\t'), id(), "Str", str());
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

std::string ListIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < children_.size(); i++) {
    childMap[absl::Substitute("child$0", i)] = children_[i]->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::Substitute("$0:ListIR", id()), childMap);
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

StatusOr<IRNode*> LambdaIR::GetDefaultExpr() {
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

std::string LambdaIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  childMap["ExpectedRelation"] =
      absl::Substitute("[$0]", absl::StrJoin(expected_column_names_, ","));
  for (auto const& x : col_exprs_) {
    childMap[absl::Substitute("ExprMap[\"$0\"]", x.name)] = x.node->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::Substitute("$0:LambdaIR", id()), childMap);
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

std::string FuncIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < args_.size(); i++) {
    childMap.emplace(absl::Substitute("arg$0", i), args_[i]->DebugString(depth + 1));
  }
  return DebugStringFmt(depth, absl::Substitute("$0:FuncIR", id()), childMap);
}

/* Float IR */
bool FloatIR::HasLogicalRepr() const { return false; }
Status FloatIR::Init(double val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string FloatIR::DebugString(int64_t depth) const {
  return absl::Substitute("$0$1:$2\t-\t$3", std::string(depth, '\t'), id(), "Float", val());
}

/* Int IR */
bool IntIR::HasLogicalRepr() const { return false; }
Status IntIR::Init(int64_t val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string IntIR::DebugString(int64_t depth) const {
  return absl::Substitute("$0$1:$2\t-\t$3", std::string(depth, '\t'), id(), "Int", val());
}

/* Bool IR */
bool BoolIR::HasLogicalRepr() const { return false; }
Status BoolIR::Init(bool val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string BoolIR::DebugString(int64_t depth) const {
  return absl::Substitute("$0$1:$2\t-\t$3", std::string(depth, '\t'), id(), "Bool", val());
}
/* Time IR */
bool TimeIR::HasLogicalRepr() const { return false; }
Status TimeIR::Init(int64_t val, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  val_ = val;
  return Status::OK();
}
std::string TimeIR::DebugString(int64_t depth) const {
  return absl::Substitute("$0$1:$2\t-\t$3", std::string(depth, '\t'), id(), "Time", val());
}

/* Metadata IR */
Status MetadataIR::Init(std::string metadata_str, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  metadata_name_ = metadata_str;
  return Status::OK();
}

Status MetadataIR::ResolveMetadataColumn(MetadataResolverIR* resolver_op,
                                         MetadataProperty* property) {
  PL_RETURN_IF_ERROR(ColumnIR::Init(property->GetColumnRepr(), ast_node()));
  resolver_ = resolver_op;
  property_ = property;
  has_metadata_resolver_ = true;
  return Status::OK();
}

std::string MetadataIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "Metadata", name());
}

/* MetadataLiteral IR */
Status MetadataLiteralIR::Init(DataIR* literal, const pypa::AstPtr& ast_node) {
  SetLineCol(ast_node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, literal));
  literal_ = literal;
  literal_type_ = literal->type();
  return Status::OK();
}

std::string MetadataLiteralIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "MetadataLiteral",
                         literal_->DebugString(0));
}

std::string MetadataResolverIR::DebugString(int64_t depth) const {
  std::vector<std::string> cols;
  for (const auto& c : metadata_columns()) {
    cols.push_back(c.first);
  }
  return absl::StrFormat("%s%d:%s\t-\tColumns:[%s]", std::string(depth, '\t'), id(),
                         "MetadataResolver", absl::StrJoin(cols, ","));
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
