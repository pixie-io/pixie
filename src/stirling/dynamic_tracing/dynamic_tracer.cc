#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

#include <string>

#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"
#include "src/stirling/dynamic_tracing/probe_transformer.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/obj_tools/proc_path_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

StatusOr<BCCProgram> CompileProgram(const ir::logical::Program& input_program) {
  PL_ASSIGN_OR_RETURN(ir::logical::Program intermediate_program,
                      TransformLogicalProgram(input_program));

  pid_t pid = bpf_tools::UProbeSpec::kDefaultPID;

  // Expect the outside caller keeps UPID, and specify them in UProbeSpec. Here upid and path are
  // specified alternatively, and upid will be replaced by binary path.
  switch (intermediate_program.binary_spec().target_oneof_case()) {
    case ir::shared::BinarySpec::TargetOneofCase::kPath:
      // Ignored, no need to resolve binary path.
      break;
    case ir::shared::BinarySpec::TargetOneofCase::kUpid: {
      pid = intermediate_program.binary_spec().upid().pid();
      // TODO(yzhao): Here the upid's PID start time is not verified, just looks for the pid.
      // We might need to add such check in the future.
      PL_ASSIGN_OR_RETURN(std::filesystem::path binary_path, obj_tools::ResolveProcExe(pid));
      intermediate_program.mutable_binary_spec()->set_path(binary_path.string());
      break;
    }
    case ir::shared::BinarySpec::TargetOneofCase::TARGET_ONEOF_NOT_SET:
      return error::InvalidArgument("Must specify path or upid");
  }

  PL_ASSIGN_OR_RETURN(ir::physical::Program physical_program, AddDwarves(intermediate_program));

  PL_ASSIGN_OR_RETURN(BCCProgram bcc_program, GenProgram(physical_program));

  if (pid != bpf_tools::UProbeSpec::kDefaultPID) {
    // TODO(yzhao): Seems with pid and binary both specified, detaching uprobes will fail after
    // the process exists. This does not happen if the pid was not specified.
    for (auto& spec : bcc_program.uprobes) {
      // When PID is specified, the binary_path must still be provided. The PID only further filters
      // which instances of the binary_path will be traced.
      spec.pid = pid;
    }
  }

  return bcc_program;
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
