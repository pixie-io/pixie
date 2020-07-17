#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"
#include "src/stirling/dynamic_tracing/probe_transformer.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

StatusOr<BCCProgram> CompileProgram(const ir::logical::Program& input_program) {
  PL_ASSIGN_OR_RETURN(ir::logical::Program intermediate_program,
                      TransformLogicalProgram(input_program));
  PL_ASSIGN_OR_RETURN(ir::physical::Program physical_program, AddDwarves(intermediate_program));
  PL_ASSIGN_OR_RETURN(BCCProgram bcc_program, GenProgram(physical_program));
  return bcc_program;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
