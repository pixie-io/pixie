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

Status IRVerifier::ExpectType(IRNodeType exp_type, const IRNode* test_node,
                              const std::string& err_msg_prefix) {
  auto actual_type = test_node->type();
  if (exp_type != actual_type) {
    auto msg =
        absl::Substitute("$0: For node with id $3, Expected $1 Got $2.", err_msg_prefix,
                         kIRNodeStrings[exp_type], test_node->type_string(), test_node->id());
    return FormatErrorMsg(msg, test_node);
  }
  return Status::OK();
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
  PL_RETURN_IF_ERROR(ExpectType(StringType, range_node->time_repr(),
                                ExpString("RangeIR", range_node->id(), "time_repr")));

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
  // TODO(philkuz) (PL-402) unhack this
  if (agg_node->by_func()->type() != IRNodeType::BoolType) {
    PL_RETURN_IF_ERROR(ExpectType(LambdaType, agg_node->by_func(),
                                  ExpString("BlockingAggIR", node->id(), "by_func")));
    // Check whether the `by` function is just a column
    auto by_func = static_cast<LambdaIR*>(agg_node->by_func());
    if (by_func->HasDictBody()) {
      return FormatErrorMsg("Expected by function to only have a column.", by_func);
    }
    PL_ASSIGN_OR_RETURN(IRNode * by_body, by_func->GetDefaultExpr());

    auto actual_type = by_body->type();
    if (ColumnType != actual_type && actual_type != ListType) {
      auto msg = absl::Substitute(
          "BlockingAggIR: For node with id $1, Expected ColumnType or ListType Got $0.",
          by_body->type_string(), by_body->id());
      return FormatErrorMsg(msg, node);
    }
  } else {
    LOG(WARNING) << "WARNING: you are currently using a hack to allow bool type for by_func";
  }
  // Check whether the `agg` fn is a dict body
  auto agg_func = static_cast<LambdaIR*>(agg_node->agg_func());
  if (!agg_func->HasDictBody()) {
    return FormatErrorMsg(
        "Expected agg function to map resulting column names to the expression that generates "
        "them.",
        agg_func);
  }
  return Status::OK();
}

Status IRVerifier::VerifyNodeConnections(IRNode* node) {
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
    default: { return Status::OK(); }
  }
}

Status IRVerifier::VerifyLineCol(IRNode* node) {
  if (!node->line_col_set()) {
    std::string err_msg = "Line and column not set for $0 with id $1.";
    return error::InvalidArgument(err_msg, node->type_string(), node->id());
  }
  return Status::OK();
}

/**
 * @brief Verifies that each node in the graph has a line, column attribute for debugging.
 *
 * @param ir_graph
 * @return const std::vector<Status>&
 */
std::vector<Status> IRVerifier::VerifyLineColGraph(const IR& ir_graph) {
  std::vector<Status> statuses;
  for (auto& i : ir_graph.dag().TopologicalSort()) {
    auto node = ir_graph.Get(i);
    Status line_col_status = VerifyLineCol(node);
    if (!line_col_status.ok()) {
      statuses.push_back(line_col_status);
    }
  }
  return statuses;
}

/**
 * @brief Verifies that each node in the graph has their connections properly initialized.
 *
 * @param ir_graph
 * @return const std::vector<Status>&
 */
std::vector<Status> IRVerifier::VerifyGraphConnections(const IR& ir_graph) {
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
  return statuses;
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
