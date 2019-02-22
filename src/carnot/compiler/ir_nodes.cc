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

void IR::DeleteEdge(int64_t from_node, int64_t to_node) { dag_.DeleteEdge(from_node, to_node); }

void IR::DeleteNode(int64_t node) { dag_.DeleteNode(node); }

std::string IR::DebugString() {
  std::string debug_string = absl::StrFormat("%s\n", dag().DebugString());
  for (auto const& a : id_node_map_) {
    debug_string += absl::StrFormat("%s\n", a.second->DebugString(0));
  }
  return debug_string;
}

void IRNode::SetLineCol(int64_t line, int64_t col) {
  line_ = line;
  col_ = col;
  line_col_set_ = true;
}

Status OperatorIR::SetParent(IRNode* node) {
  if (!node->IsOp()) {
    return error::InvalidArgument("Expected Op, got $0 instead", node->type_string());
  }
  parent_ = static_cast<OperatorIR*>(node);
  return Status::OK();
}

bool MemorySourceIR::HasLogicalRepr() const { return true; }

Status MemorySourceIR::Init(IRNode* table_node, IRNode* select) {
  table_node_ = table_node;
  select_ = select;
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, select_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, table_node_));

  return Status::OK();
}

Status MemorySourceIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::MemorySourceOperator();
  DCHECK(table_node_->type() == IRNodeType::StringType);
  pb->set_name(static_cast<StringIR*>(table_node_)->str());

  if (!columns_set()) {
    return error::InvalidArgument("MemorySource columns are not set.");
  }

  for (const auto& col : columns_) {
    pb->add_column_idxs(col->col_idx());
    pb->add_column_names(col->col_name());
    pb->add_column_types(col->type());
  }

  if (IsTimeSet()) {
    auto start_time = new ::google::protobuf::Int64Value();
    start_time->set_value(time_start_ms_);
    pb->set_allocated_start_time(start_time);
    auto stop_time = new ::google::protobuf::Int64Value();
    stop_time->set_value(time_stop_ms_);
    pb->set_allocated_stop_time(stop_time);
  }

  op->set_op_type(carnotpb::MEMORY_SOURCE_OPERATOR);
  op->set_allocated_mem_source_op(pb);
  return Status::OK();
}

std::string DebugStringFmt(int64_t depth, std::string name,
                           std::map<std::string, std::string> property_value_map) {
  std::vector<std::string> property_strings;
  std::map<std::string, std::string>::iterator it;
  std::string depth_string = std::string(depth, '\t');
  property_strings.push_back(absl::StrFormat("%s%s", depth_string, name));

  for (it = property_value_map.begin(); it != property_value_map.end(); it++) {
    std::string prop_str = absl::Substitute("$0 $1\t-$2", depth_string, it->first, it->second);
    property_strings.push_back(prop_str);
  }
  return absl::StrJoin(property_strings, "\n");
}
std::string MemorySourceIR::DebugString(int64_t depth) const {
  return DebugStringFmt(
      depth, absl::StrFormat("%d:MemorySourceIR", id()),
      {{"From", table_node_->DebugString(depth + 1)}, {"Select", select_->DebugString(depth + 1)}});
}

bool MemorySinkIR::HasLogicalRepr() const { return true; }

Status MemorySinkIR::Init(IRNode* parent_node, const std::string& name) {
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  name_ = name;
  name_set_ = true;

  return Status::OK();
}

std::string MemorySinkIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:MemorySinkIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)}});
}

Status MemorySinkIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::MemorySinkOperator();
  pb->set_name(name_);

  auto types = relation().col_types();
  auto names = relation().col_names();

  for (size_t i = 0; i < relation().NumColumns(); i++) {
    pb->add_column_types(types[i]);
    pb->add_column_names(names[i]);
  }

  op->set_op_type(carnotpb::MEMORY_SINK_OPERATOR);
  op->set_allocated_mem_sink_op(pb);
  return Status::OK();
}

Status RangeIR::Init(IRNode* parent_node, IRNode* time_repr) {
  // TODO(philkuz) implement string to ms (int) conversion.
  time_repr_ = time_repr;
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, time_repr_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  return Status::OK();
}

bool RangeIR::HasLogicalRepr() const { return false; }

std::string RangeIR::DebugString(int64_t depth) const {
  return DebugStringFmt(
      depth, absl::StrFormat("%d:RangeIR", id()),
      {{"Parent", parent()->DebugString(depth + 1)}, {"Time", time_repr_->DebugString(depth + 1)}});
}

Status RangeIR::ToProto(carnotpb::Operator*) const {
  return error::InvalidArgument("RangeIR has no protobuf representation.");
}

Status MapIR::Init(IRNode* parent_node, IRNode* lambda_func) {
  // TODO(philkuz) implement string to ms (int) conversion.
  lambda_func_ = lambda_func;
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, lambda_func_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  return Status::OK();
}

bool MapIR::HasLogicalRepr() const { return true; }

std::string MapIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:MapIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"Lambda", lambda_func_->DebugString(depth + 1)}});
}

Status MapIR::EvaluateExpression(carnotpb::ScalarExpression* expr, const IRNode& ir_node) const {
  switch (ir_node.type()) {
    case IRNodeType::ColumnType: {
      auto col = expr->mutable_column();
      col->set_node(parent()->id());
      col->set_index(static_cast<const ColumnIR&>(ir_node).col_idx());
      break;
    }
    case IRNodeType::FuncType: {
      auto func = expr->mutable_func();
      auto casted_ir = static_cast<const FuncIR&>(ir_node);
      func->set_name(casted_ir.func_name());
      for (const auto& arg : casted_ir.args()) {
        auto func_arg = func->add_args();
        PL_RETURN_IF_ERROR(EvaluateExpression(func_arg, *arg));
      }
      break;
    }
    case IRNodeType::IntType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const IntIR&>(ir_node);
      value->set_data_type(types::DataType::INT64);
      value->set_int64_value(casted_ir.val());
      break;
    }
    case IRNodeType::StringType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const StringIR&>(ir_node);
      value->set_data_type(types::DataType::STRING);
      value->set_string_value(casted_ir.str());
      break;
    }
    case IRNodeType::FloatType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const FloatIR&>(ir_node);
      value->set_data_type(types::DataType::FLOAT64);
      value->set_float64_value(casted_ir.val());
      break;
    }
    case IRNodeType::BoolType: {
      auto value = expr->mutable_constant();
      auto casted_ir = static_cast<const BoolIR&>(ir_node);
      value->set_data_type(types::DataType::BOOLEAN);
      value->set_bool_value(casted_ir.val());
      break;
    }
    default: {
      return error::InvalidArgument("Didn't expect node of type $0 in expression evaluator.",
                                    ir_node.type_string());
    }
  }
  return Status::OK();
}

Status MapIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::MapOperator();

  for (const auto& kv : col_expr_map_) {
    auto expr = pb->add_expressions();
    PL_RETURN_IF_ERROR(EvaluateExpression(expr, *kv.second));
    pb->add_column_names(kv.first);
  }

  op->set_op_type(carnotpb::MAP_OPERATOR);
  op->set_allocated_map_op(pb);
  return Status::OK();
}

Status AggIR::Init(IRNode* parent_node, IRNode* by_func, IRNode* agg_func) {
  // TODO(philkuz) expect by_func to be a lambda node once the types diff is in.
  by_func_ = by_func;

  // TODO(philkuz) expect by_func to be a lambda node or funcname node once the types diff is in.
  agg_func_ = agg_func;
  PL_RETURN_IF_ERROR(SetParent(parent_node));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, agg_func_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, by_func_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent(), this));
  return Status();
}

bool AggIR::HasLogicalRepr() const { return true; }

std::string AggIR::DebugString(int64_t depth) const {
  return DebugStringFmt(depth, absl::StrFormat("%d:AggIR", id()),
                        {{"Parent", parent()->DebugString(depth + 1)},
                         {"ByFn", by_func_->DebugString(depth + 1)},
                         {"AggFn", agg_func_->DebugString(depth + 1)}});
}

Status AggIR::EvaluateAggregateExpression(carnotpb::AggregateExpression* expr,
                                          const IRNode& ir_node) const {
  DCHECK(ir_node.type() == IRNodeType::FuncType);
  auto casted_ir = static_cast<const FuncIR&>(ir_node);
  expr->set_name(casted_ir.func_name());
  for (auto ir_arg : casted_ir.args()) {
    auto arg_pb = expr->add_args();
    switch (ir_arg->type()) {
      case IRNodeType::ColumnType: {
        auto col = arg_pb->mutable_column();
        col->set_node(parent()->id());
        col->set_index(static_cast<ColumnIR*>(ir_arg)->col_idx());
        break;
      }
      case IRNodeType::IntType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<IntIR*>(ir_arg);
        value->set_data_type(types::DataType::INT64);
        value->set_int64_value(casted_ir->val());
        break;
      }
      case IRNodeType::StringType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<StringIR*>(ir_arg);
        value->set_data_type(types::DataType::STRING);
        value->set_string_value(casted_ir->str());
        break;
      }
      case IRNodeType::FloatType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<FloatIR*>(ir_arg);
        value->set_data_type(types::DataType::FLOAT64);
        value->set_float64_value(casted_ir->val());
        break;
      }
      case IRNodeType::BoolType: {
        auto value = arg_pb->mutable_constant();
        auto casted_ir = static_cast<BoolIR*>(ir_arg);
        value->set_data_type(types::DataType::BOOLEAN);
        value->set_bool_value(casted_ir->val());
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

Status AggIR::ToProto(carnotpb::Operator* op) const {
  auto pb = new carnotpb::BlockingAggregateOperator();

  for (const auto& kv : agg_val_map_) {
    auto expr = pb->add_values();
    PL_RETURN_IF_ERROR(EvaluateAggregateExpression(expr, *kv.second));
    pb->add_value_names(kv.first);
  }

  for (const auto& group : groups_) {
    auto group_pb = pb->add_groups();
    group_pb->set_node(parent()->id());
    group_pb->set_index(group->col_idx());
    pb->add_group_names(group->col_name());
  }

  op->set_op_type(carnotpb::BLOCKING_AGGREGATE_OPERATOR);
  op->set_allocated_blocking_agg_op(pb);
  return Status::OK();
}

bool ColumnIR::HasLogicalRepr() const { return false; }
Status ColumnIR::Init(std::string col_name) {
  col_name_ = col_name;
  return Status::OK();
}

std::string ColumnIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "Column", col_name());
}

bool StringIR::HasLogicalRepr() const { return false; }
Status StringIR::Init(std::string str) {
  str_ = str;
  return Status::OK();
}

std::string StringIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "Str", str());
}

bool ListIR::HasLogicalRepr() const { return false; }

Status ListIR::AddListItem(IRNode* node) {
  children_.push_back(node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, node));
  return Status::OK();
}
std::string ListIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < children_.size(); i++) {
    childMap[absl::StrFormat("child%d", i)] = children_[i]->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::StrFormat("%d:ListIR", id()), childMap);
}

bool LambdaIR::HasLogicalRepr() const { return false; }
bool LambdaIR::HasDictBody() const { return has_dict_body_; }

Status LambdaIR::Init(std::unordered_set<std::string> expected_column_names,
                      ColExprMap col_expr_map) {
  expected_column_names_ = expected_column_names;
  col_expr_map_ = col_expr_map;
  has_dict_body_ = true;
  return Status::OK();
}

Status LambdaIR::Init(std::unordered_set<std::string> expected_column_names, IRNode* node) {
  expected_column_names_ = expected_column_names;
  col_expr_map_[default_key] = node;
  has_dict_body_ = false;
  return Status::OK();
}

StatusOr<IRNode*> LambdaIR::GetDefaultExpr() {
  if (HasDictBody()) {
    return error::InvalidArgument(
        "Couldn't return the default expression, Lambda initialized as dict.");
  }
  return col_expr_map_[default_key];
}

std::string LambdaIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  // TODO(philkuz) figure out expected relation.
  childMap["ExpectedRelation"] =
      absl::StrFormat("[%s]", absl::StrJoin(expected_column_names_, ","));
  for (auto const& x : col_expr_map_) {
    childMap[absl::StrFormat("ExprMap[\"%s\"]", x.first)] = x.second->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::StrFormat("%d:LambdaIR", id()), childMap);
}

bool FuncIR::HasLogicalRepr() const { return false; }
Status FuncIR::Init(std::string func_name, std::vector<IRNode*> args) {
  func_name_ = func_name;
  args_ = args;
  return Status::OK();
}

std::string FuncIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < args_.size(); i++) {
    childMap[absl::StrFormat("arg%d", i)] = args_[i]->DebugString(depth + 1);
  }
  return DebugStringFmt(depth, absl::StrFormat("%d:FuncIR", id()), childMap);
}

/* Float IR */
bool FloatIR::HasLogicalRepr() const { return false; }
Status FloatIR::Init(double val) {
  val_ = val;
  return Status::OK();
}
std::string FloatIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%f", std::string(depth, '\t'), id(), "Float", val());
}

/* Int IR */
bool IntIR::HasLogicalRepr() const { return false; }
Status IntIR::Init(int64_t val) {
  val_ = val;
  return Status::OK();
}
std::string IntIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%d", std::string(depth, '\t'), id(), "Int", val());
}

/* Bool IR */
bool BoolIR::HasLogicalRepr() const { return false; }
Status BoolIR::Init(bool val) {
  val_ = val;
  return Status::OK();
}
std::string BoolIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%d", std::string(depth, '\t'), id(), "Bool", val());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
