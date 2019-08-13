/**
 * @brief Verifies the ir graph structure after construction to make sure nodes connect in the right
 * way. Does not verify any data types.
 *
 */
#pragma once
#include <string>
#include <vector>

#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

class IRVerifier {
 public:
  /**
   * @brief Verifies that each node in the graph has a line, column attribute for debugging.
   *
   * @param ir_graph
   * @return const Status&
   */
  Status VerifyLineColGraph(const IR& ir_graph);

  /**
   * @brief Verifies that each node in the graph has their connections properly initialized.
   *
   * @param ir_graph
   * @return const Status&
   */
  Status VerifyGraphConnections(const IR& ir_graph);

 private:
  bool TypeIsOperator(IRNodeType type);

  Status ExpectType(IRNodeType exp_type, const IRNode* test_node,
                    const std::string& err_msg_prefix);
  Status ExpectType(std::vector<IRNodeType> possible_types, const IRNode* test_node,
                    const std::string& err_msg_prefix);
  Status ExpectOp(IRNode* test_node, std::string err_msg_prefix);
  std::string ExpString(const std::string& node_name, int64_t id, const std::string& property_name);
  Status VerifyMemorySource(MemorySourceIR* node);

  Status VerifyRange(RangeIR* node);

  Status VerifyMap(MapIR* node);

  Status VerifyFilter(FilterIR* node);

  Status VerifyLimit(LimitIR* node);

  Status VerifyBlockingAgg(BlockingAggIR* node);

  Status VerifySink(MemorySinkIR* node);

  Status VerifyNodeConnections(IRNode* node);
  Status VerifyLineCol(IRNode* node);
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
