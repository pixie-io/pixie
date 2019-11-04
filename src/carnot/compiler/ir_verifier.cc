#include "src/carnot/compiler/ir_verifier.h"

#include <string>
#include <vector>

#include "src/carnot/compiler/compiler_error_context/compiler_error_context.h"
#include "src/carnot/compiler/ir/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

Status IRVerifier::ExpectType(std::vector<IRNodeType> possible_types, const IRNode* test_node,
                              const std::string& err_msg_prefix) {
  std::vector<std::string> missing_exp_types_strings;
  auto actual_type = test_node->type();
  for (auto exp_type : possible_types) {
    if (exp_type == actual_type) {
      return Status::OK();
    }
    missing_exp_types_strings.push_back(kIRNodeStrings[static_cast<int64_t>(exp_type)]);
  }
  auto msg = absl::Substitute("$0: For node with id $3, Expected [$1] Got $2.", err_msg_prefix,
                              absl::StrJoin(missing_exp_types_strings, "\n"),
                              test_node->type_string(), test_node->id());
  return test_node->CreateIRNodeError(msg);
}

Status IRVerifier::ExpectType(IRNodeType exp_type, const IRNode* test_node,
                              const std::string& err_msg_prefix) {
  return ExpectType(std::vector<IRNodeType>({exp_type}), test_node, err_msg_prefix);
}

std::string IRVerifier::ExpString(const std::string& node_name, const int64_t id,
                                  const std::string& property_name) {
  return absl::Substitute("$0(id=$1) '$2'", node_name, id, property_name);
}
Status IRVerifier::VerifyMemorySource(MemorySourceIR*) { return Status::OK(); }

Status IRVerifier::VerifyRange(RangeIR* range_node) {
  PL_RETURN_IF_ERROR(ExpectType({IRNodeType::kInt, IRNodeType::kFunc, IRNodeType::kString},
                                range_node->start_repr(),
                                ExpString("RangeIR", range_node->id(), "start_repr")));
  PL_RETURN_IF_ERROR(ExpectType({IRNodeType::kInt, IRNodeType::kFunc, IRNodeType::kString},
                                range_node->stop_repr(),
                                ExpString("RangeIR", range_node->id(), "stop_repr")));

  PL_RETURN_IF_ERROR(ExpectType(IRNodeType::kMemorySource, range_node->parents()[0],
                                ExpString("RangeIR", range_node->id(), "parent")));
  return Status::OK();
}

Status IRVerifier::VerifyMap(MapIR*) { return Status::OK(); }

Status IRVerifier::VerifyFilter(FilterIR*) { return Status::OK(); }

Status IRVerifier::VerifyLimit(LimitIR*) { return Status::OK(); }

Status IRVerifier::VerifySink(MemorySinkIR* sink_node) {
  if (!sink_node->name_set()) {
    return sink_node->CreateIRNodeError("Expected sink to have name set.");
  }
  return Status::OK();
}

Status IRVerifier::VerifyBlockingAgg(BlockingAggIR*) { return Status::OK(); }

Status IRVerifier::VerifyNodeConnections(IRNode* node) {
  // Should only look at ops.
  if (!node->IsOperator()) {
    return Status::OK();
  }
  switch (node->type()) {
    case IRNodeType::kMemorySource: {
      return VerifyMemorySource(static_cast<MemorySourceIR*>(node));
    }
    case IRNodeType::kRange: {
      return VerifyRange(static_cast<RangeIR*>(node));
    }
    case IRNodeType::kMap: {
      return VerifyMap(static_cast<MapIR*>(node));
    }
    case IRNodeType::kBlockingAgg: {
      return VerifyBlockingAgg(static_cast<BlockingAggIR*>(node));
    }
    case IRNodeType::kMemorySink: {
      return VerifySink(static_cast<MemorySinkIR*>(node));
    }
    case IRNodeType::kFilter: {
      return VerifyFilter(static_cast<FilterIR*>(node));
    }
    case IRNodeType::kLimit: {
      return VerifyLimit(static_cast<LimitIR*>(node));
    }
    case IRNodeType::kJoin: {
      return Status::OK();
    }
    default: {
      return node->CreateIRNodeError("Couldn't find verify node of type $0", node->type_string());
    }
  }
}

Status IRVerifier::VerifyLineCol(IRNode* node) {
  if (!node->line_col_set()) {
    std::string err_msg = "Line and column not set for $0 with id $1. DBG string: $2";
    return error::InvalidArgument(err_msg, node->type_string(), node->id(), node->DebugString());
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
  return MergeStatuses(statuses);
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
    if (node->type() == IRNodeType::kMemorySink) {
      has_sink = true;
    }
  }
  if (!has_sink) {
    statuses.push_back(
        error::InvalidArgument("No Result() call found in the query. You must end the query with a "
                               "Result call to save something out."));
  }

  return MergeStatuses(statuses);
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
