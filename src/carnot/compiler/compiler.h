#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/planpb/plan.pb.h"

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
  StatusOr<planpb::Plan> Compile(const std::string& query, CompilerState* compiler_state);
  StatusOr<std::shared_ptr<IR>> CompileToIR(const std::string& query,
                                            CompilerState* compiler_state);

 private:
  StatusOr<std::shared_ptr<IR>> QueryToIR(const std::string& query, CompilerState* compiler_state);

  Status UpdateColumnsAndVerifyUDFs(IR* ir, CompilerState* compiler_state);
  Status VerifyIRConnections(const IR& ir);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
