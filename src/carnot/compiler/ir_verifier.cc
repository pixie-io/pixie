#include "src/carnot/compiler/ir_verifier.h"

#include <string>
#include <vector>

#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {
Status IRVerifier::FormatErrorMsg(const std::string& err_msg, const IRNode* node) {
  return error::InvalidArgument("Line $0, Col $1 : $2", node->line(), node->col(), err_msg);
}

Status IRVerifier::ExpectType(std::vector<IRNodeType> possible_types, const IRNode* test_node,
                              const std::string& err_msg_prefix) {
  std::vector<std::string> missing_exp_types_strings;
  auto actual_type = test_node->type();
  for (auto exp_type : possible_types) {
    if (exp_type == actual_type) {
      return Status::OK();
    }
    missing_exp_types_strings.push_back(kIRNodeStrings[exp_type]);
  }
  auto msg = absl::Substitute("$0: For node with id $3, Expected [$1] Got $2.", err_msg_prefix,
                              absl::StrJoin(missing_exp_types_strings, "\n"),
                              test_node->type_string(), test_node->id());
  return FormatErrorMsg(msg, test_node);
}

Status IRVerifier::ExpectType(IRNodeType exp_type, const IRNode* test_node,
                              const std::string& err_msg_prefix) {
  return ExpectType(std::vector<IRNodeType>({exp_type}), test_node, err_msg_prefix);
}

Status IRVerifier::ExpectOp(IRNode* test_node, std::string err_msg_prefix) {
  if (!test_node->IsOp()) {
    auto msg = absl::Substitute("$0: Expected an Operator Got $1", err_msg_prefix,
                                test_node->type_string());
    return FormatErrorMsg(msg, test_node);
  }
  return Status::OK();
}

std::string IRVerifier::ExpString(const std::string& node_name, const int64_t id,
                                  const std::string& property_name) {
  return absl::Substitute("$0(id=$1) '$2'", node_name, id, property_name);
}
Status IRVerifier::VerifyMemorySource(IRNode* node) {
  auto mem_node = static_cast<MemorySourceIR*>(node);
  PL_RETURN_IF_ERROR(ExpectType(StringType, mem_node->table_node(),
                                ExpString("MemorySourceIR", node->id(), "table")));
  PL_RETURN_IF_ERROR(
      ExpectType(ListType, mem_node->select(), ExpString("MemorySourceIR", node->id(), "select")));

  auto select_node = static_cast<ListIR*>(mem_node->select());
  for (auto& c : select_node->children()) {
    PL_RETURN_IF_ERROR(
        ExpectType(StringType, c, ExpString("MemorySourceIR select", select_node->id(), "child")));
  }
  return Status::OK();
}

Status IRVerifier::VerifyRange(IRNode* node) {
  auto range_node = static_cast<RangeIR*>(node);
  PL_RETURN_IF_ERROR(ExpectType({IntType, FuncType, StringType}, range_node->start_repr(),
                                ExpString("RangeIR", range_node->id(), "start_repr")));
  PL_RETURN_IF_ERROR(ExpectType({IntType, FuncType, StringType}, range_node->stop_repr(),
                                ExpString("RangeIR", range_node->id(), "stop_repr")));

  PL_RETURN_IF_ERROR(ExpectType(MemorySourceType, range_node->parent(),
                                ExpString("RangeIR", range_node->id(), "parent")));
  return Status::OK();
}

Status IRVerifier::VerifyMap(IRNode* node) {
  auto map_node = static_cast<MapIR*>(node);
  PL_RETURN_IF_ERROR(ExpectType(LambdaType, map_node->lambda_func(),
                                ExpString("MapIR", node->id(), "lambda_func")));
  PL_RETURN_IF_ERROR(ExpectOp(map_node->parent(), ExpString("MapIR", node->id(), "parent")));

  // verify properties of the lambda_func
  auto lambda_func = static_cast<LambdaIR*>(map_node->lambda_func());

  if (!lambda_func->HasDictBody()) {
    return FormatErrorMsg("Expected lambda func to have dictionary body.", lambda_func);
  }
  return Status::OK();
}

Status IRVerifier::VerifyFilter(IRNode* node) {
  auto filter_node = static_cast<FilterIR*>(node);
  PL_RETURN_IF_ERROR(ExpectType(LambdaType, filter_node->filter_func(),
                                ExpString("FilterIR", node->id(), "filter_func")));
  PL_RETURN_IF_ERROR(ExpectOp(filter_node->parent(), ExpString("FilterIR", node->id(), "parent")));

  // verify properties of the filter_func
  auto filter_func = static_cast<LambdaIR*>(filter_node->filter_func());

  if (filter_func->HasDictBody()) {
    return FormatErrorMsg("Expected filter function to only contain an expression.", filter_func);
  }
  return Status::OK();
}

Status IRVerifier::VerifyLimit(IRNode* node) {
  auto limit_node = static_cast<LimitIR*>(node);
  PL_RETURN_IF_ERROR(ExpectType(IntType, limit_node->limit_node(),
                                ExpString("LimitIR", node->id(), "limit_node")));
  PL_RETURN_IF_ERROR(ExpectOp(limit_node->parent(), ExpString("LimitIR", node->id(), "parent")));

  return Status::OK();
}

Status IRVerifier::VerifySink(IRNode* node) {
  auto sink_node = static_cast<MemorySinkIR*>(node);
  PL_RETURN_IF_ERROR(
      ExpectOp(sink_node->parent(), ExpString("MemorySinkIR", node->id(), "parent")));

  if (!sink_node->name_set()) {
    return FormatErrorMsg("Expected sink to have name set.", sink_node);
  }
  return Status::OK();
}

Status IRVerifier::VerifyBlockingAgg(IRNode* node) {
  auto agg_node = static_cast<BlockingAggIR*>(node);
  PL_RETURN_IF_ERROR(ExpectType(LambdaType, agg_node->agg_func(),
                                ExpString("BlockingAggIR", node->id(), "agg_func")));
  PL_RETURN_IF_ERROR(
      ExpectOp(agg_node->parent(), ExpString("BlockingAggIR", node->id(), "parent")));
  // Only check if by_func is not a nullptr.
  if (agg_node->by_func() != nullptr) {
    PL_RETURN_IF_ERROR(ExpectType(LambdaType, agg_node->by_func(),
                                  ExpString("BlockingAggIR", node->id(), "by_func")));
    // Check whether the `by` function is just a column
    auto by_func = static_cast<LambdaIR*>(agg_node->by_func());
    if (by_func->HasDictBody()) {
      return FormatErrorMsg("Expected by function to only contain a column.", by_func);
    }
    PL_ASSIGN_OR_RETURN(IRNode * by_body, by_func->GetDefaultExpr());

    auto actual_type = by_body->type();
    if (ColumnType != actual_type && actual_type != ListType) {
      auto msg = absl::Substitute(
          "BlockingAggIR: For node with id $1, Expected ColumnType or ListType Got $0.",
          by_body->type_string(), by_body->id());
      return FormatErrorMsg(msg, node);
    }
  } else if (agg_node->groups_set() || !agg_node->groups().empty()) {
    // Groups shouldn't be set.
    return FormatErrorMsg("AggIR: by function is not set, shouldn't have groups set.", node);
  }

  // Check whether the `agg` fn is a dict body
  auto agg_func = static_cast<LambdaIR*>(agg_node->agg_func());
  if (!agg_func->HasDictBody()) {
    return FormatErrorMsg(
        "Expected agg function to map resulting column names to the expression that generates "
        "them.",
        agg_func);
  }
  ColExpressionVector col_exprs = agg_func->col_exprs();
  for (const auto& entry : col_exprs) {
    // check that the expression type is a function and that it only has leaf nodes as children.
    if (entry.node->type() != IRNodeType::FuncType) {
      return FormatErrorMsg(
          absl::Substitute("Expected agg fns of the format \"udf(r.column_name)\". Object "
                           "of type $0 not allowed.",
                           entry.node->type_string()),
          entry.node);
    }
    auto func = static_cast<FuncIR*>(entry.node);
    for (const auto& fn_child : func->args()) {
      if (fn_child->type() == IRNodeType::FuncType) {
        return FormatErrorMsg("Nested aggregate expressions not allowed.", fn_child);
      }
    }
  }
  return Status::OK();
}

Status IRVerifier::VerifyNodeConnections(IRNode* node) {
  // Should only look at ops.
  if (!node->IsOp()) {
    return Status::OK();
  }
  switch (node->type()) {
    case IRNodeType::MemorySourceType: {
      return VerifyMemorySource(node);
    }
    case IRNodeType::RangeType: {
      return VerifyRange(node);
    }
    case IRNodeType::MapType: {
      return VerifyMap(node);
    }
    case IRNodeType::BlockingAggType: {
      return VerifyBlockingAgg(node);
    }
    case IRNodeType::MemorySinkType: {
      return VerifySink(node);
    }
    case IRNodeType::FilterType: {
      return VerifyFilter(node);
    }
    case IRNodeType::LimitType: {
      return VerifyLimit(node);
    }
    default: {
      return IRUtils::CreateIRNodeError(
          absl::StrFormat("Couldn't find verify node of type %s", node->type_string()), *node);
    }
  }
}

Status IRVerifier::VerifyLineCol(IRNode* node) {
  if (!node->line_col_set()) {
    std::string err_msg = "Line and column not set for $0 with id $1. DBG string: $2";
    return error::InvalidArgument(err_msg, node->type_string(), node->id(), node->DebugString(0));
  }
  return Status::OK();
}

/**
 * @brief Verifies that each node in the graph has a line, column attribute for debugging.
 *
 * @param ir_graph
 * @return const std::vector<Status>&
 */
Status IRVerifier::VerifyLineColGraph(const IR& ir_graph) {
  std::vector<Status> statuses;
  for (auto& i : ir_graph.dag().TopologicalSort()) {
    auto node = ir_graph.Get(i);
    Status line_col_status = VerifyLineCol(node);
    if (!line_col_status.ok()) {
      statuses.push_back(line_col_status);
    }
  }
  return CombineStatuses(statuses);
}

Status IRVerifier::CombineStatuses(const std::vector<Status>& statuses) {
  if (!statuses.empty()) {
    std::vector<std::string> msgs;
    for (const auto& s : statuses) {
      msgs.push_back(s.msg());
    }
    return Status(statuses[0].code(), absl::StrJoin(msgs, "\n"));
  }
  return Status::OK();
}

/**
 * @brief Verifies that each node in the graph has their connections properly initialized.
 *
 * @param ir_graph
 * @return const std::vector<Status>&
 */
Status IRVerifier::VerifyGraphConnections(const IR& ir_graph) {
  std::vector<Status> statuses;
  bool has_sink = false;
  for (auto& i : ir_graph.dag().TopologicalSort()) {
    auto node = ir_graph.Get(i);
    Status cur_status = VerifyNodeConnections(node);
    if (!cur_status.ok()) {
      statuses.push_back(cur_status);
    }
    if (node->type() == IRNodeType::MemorySinkType) {
      has_sink = true;
    }
  }
  if (!has_sink) {
    statuses.push_back(
        error::InvalidArgument("No Result() call found in the query. You must end the query with a "
                               "Result call to save something out."));
  }
  return CombineStatuses(statuses);
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
