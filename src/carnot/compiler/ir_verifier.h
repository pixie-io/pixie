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
   * @return const std::vector<Status>&
   */
  std::vector<Status> VerifyLineColGraph(IR* ir_graph);

  /**
   * @brief Verifies that each node in the graph has their connections properly initialized.
   *
   * @param ir_graph
   * @return const std::vector<Status>&
   */
  std::vector<Status> VerifyGraphConnections(IR* ir_graph);

 private:
  bool TypeIsOp(IRNodeType type);
  Status FormatErrorMsg(const std::string& err_msg, const IRNode* node);

  Status ExpectType(IRNodeType exp_type, const IRNode* act_val, const std::string& parent_string);

  Status ExpectOp(IRNode* act_val, std::string parent_string);
  std::string ExpString(const std::string& node_name, const int64_t id,
                        const std::string& property_name);
  Status VerifyMemorySource(IRNode* node);

  Status VerifyRange(IRNode* node);

  Status VerifyMap(IRNode* node);

  Status VerifyAgg(IRNode* node);

  Status VerifySink(IRNode* node);

  Status VerifyNodeConnections(IRNode* node);
  Status VerifyLineCol(IRNode* node);
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
