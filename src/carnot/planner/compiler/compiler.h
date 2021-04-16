#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planpb/plan.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * The compiler takes a query in the form of a string and compiles it into a logical plan.
 */
class Compiler {
 public:
  /**
   * Compile the query into a logical plan.
   * @param query the query to compile
   * @param compiler_state compiler state
   * @param exec_funcs list of funcs to execute.
   * @return the logical plan in the form of a plan protobuf message.
   */
  StatusOr<planpb::Plan> Compile(const std::string& query, CompilerState* compiler_state,
                                 const ExecFuncs& exec_funcs);
  StatusOr<planpb::Plan> Compile(const std::string& query, CompilerState* compiler_state);
  StatusOr<std::shared_ptr<IR>> CompileToIR(const std::string& query, CompilerState* compiler_state,
                                            const ExecFuncs& exec_funcs);
  StatusOr<std::shared_ptr<IR>> CompileToIR(const std::string& query,
                                            CompilerState* compiler_state);

  /**
   * @brief Compiles the query to a Trace
   *
   * @param query the query to compile
   * @param compiler_state compiler state
   * @param exec_funcs list of funcs to execute.
   * @return the IR for the dynamic trace.
   */
  StatusOr<std::unique_ptr<MutationsIR>> CompileTrace(const std::string& query,
                                                      CompilerState* compiler_state,
                                                      const ExecFuncs& exec_funcs);
  StatusOr<shared::scriptspb::FuncArgsSpec> GetMainFuncArgsSpec(const std::string& query,
                                                                CompilerState* compiler_state);
  StatusOr<px::shared::scriptspb::VisFuncsInfo> GetVisFuncsInfo(const std::string& query,
                                                                CompilerState* compiler_state);

 private:
  StatusOr<std::shared_ptr<IR>> QueryToIR(const std::string& query, CompilerState* compiler_state,
                                          const ExecFuncs& exec_funcs);

  Status Analyze(IR* ir, CompilerState* compiler_state);
  Status Optimize(IR* ir, CompilerState* compiler_state);
  Status VerifyGraphHasResultSink(IR* ir);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
