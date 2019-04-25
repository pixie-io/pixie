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
  bool TypeIsOp(IRNodeType type);
  // TODO(philkuz) refactor to use IRUtils::CreateIRNodeError.
  Status FormatErrorMsg(const std::string& err_msg, const IRNode* node);

  Status ExpectType(IRNodeType exp_type, const IRNode* test_node,
                    const std::string& err_msg_prefix);
  Status ExpectType(std::vector<IRNodeType> possible_types, const IRNode* test_node,
                    const std::string& err_msg_prefix);
  Status ExpectOp(IRNode* test_node, std::string err_msg_prefix);
  std::string ExpString(const std::string& node_name, int64_t id, const std::string& property_name);
  Status VerifyMemorySource(IRNode* node);

  Status VerifyRange(IRNode* node);

  Status VerifyMap(IRNode* node);

  Status VerifyFilter(IRNode* node);

  Status VerifyLimit(IRNode* node);

  Status VerifyBlockingAgg(IRNode* node);

  Status VerifySink(IRNode* node);

  Status VerifyNodeConnections(IRNode* node);
  Status VerifyLineCol(IRNode* node);
  Status CombineStatuses(const std::vector<Status>& statuses);
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
