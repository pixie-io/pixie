#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace compiler {

/**
 * The compiler takes a query in the form of a string and compiles it into a logical plan.
 */
class Compiler {
 public:
  /**
   * Compile the query into a logical plan.
   * @param query the query to compile.
   * @return the logical plan in the form of a plan protobuf message.
   */
  StatusOr<carnotpb::Plan> Compile(const std::string& query, CompilerState* compiler_state);

 private:
  StatusOr<std::shared_ptr<IR>> QueryToIR(const std::string& query);
  StatusOr<carnotpb::Plan> IRToLogicalPlan(std::shared_ptr<IR> ir);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
