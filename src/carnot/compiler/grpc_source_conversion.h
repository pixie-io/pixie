#pragma once
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {
using table_store::schema::Relation;

class GRPCSourceGroupConversionRule : public Rule {
  /**
   * @brief GRPCSourceGroupConversionRule converts GRPCSourceGroups into a union of GRPCGroups.
   */

 public:
  GRPCSourceGroupConversionRule() : Rule(nullptr) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override;
  StatusOr<bool> ExpandGRPCSourceGroup(GRPCSourceGroupIR* group_ir);

  /**
   * @brief Creates a GRPC source given the group_ir and the remote_id.
   *
   * @param group_ir: the node to get the relation and line, col from.
   * @param remote_id: the string remote id.
   * @return StatusOr<GRPCSourceIR*>:  the GRPCSourceIR;
   */
  StatusOr<GRPCSourceIR*> CreateGRPCSource(GRPCSourceGroupIR* group_ir,
                                           const std::string& remote_id);

  /**
   * @brief Converts the group ir into either a single GRPCSource or a union of GRPPCSources,
   * depending on how many sinks feed into a group.
   *
   * @param group_ir the group ir to feed in.
   * @return StatusOr<OperatorIR*>: the representative node for the group_ir.
   */
  StatusOr<OperatorIR*> ConvertGRPCSourceGroup(GRPCSourceGroupIR* group_ir);

  Status RemoveGRPCSourceGroup(GRPCSourceGroupIR* grpc_source_group) const;
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
