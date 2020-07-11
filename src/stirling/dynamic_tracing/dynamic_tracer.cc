#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

#include <string>
#include <vector>

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

Status DeployBCCProgram(const dynamic_tracing::BCCProgram& bcc_program,
                        bpf_tools::BCCWrapper* bcc_wrapper) {
  PL_RETURN_IF_ERROR(bcc_wrapper->InitBPFProgram(bcc_program.code));

  for (const auto& uprobe : bcc_program.uprobes) {
    PL_RETURN_IF_ERROR(bcc_wrapper->AttachUProbe(uprobe));
    // TODO(yzhao): Also open the perf buffers.
  }

  return Status::OK();
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
