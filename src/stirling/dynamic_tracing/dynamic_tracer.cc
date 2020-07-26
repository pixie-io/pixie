#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/system/system.h"

#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/dynamic_tracing/dwarf_info.h"
#include "src/stirling/dynamic_tracing/probe_transformer.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/obj_tools/obj_tools.h"
#include "src/stirling/obj_tools/proc_path_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;

namespace {

StatusOr<std::vector<UProbeSpec>> GetUProbeSpec(const ir::shared::BinarySpec& binary_spec,
                                                const ir::physical::Probe& probe,
                                                elf_tools::ElfReader* elf_reader) {
  UProbeSpec spec;

  spec.binary_path = binary_spec.path();
  spec.symbol = probe.trace_point().symbol();
  DCHECK(probe.trace_point().type() == ir::shared::TracePoint::ENTRY ||
         probe.trace_point().type() == ir::shared::TracePoint::RETURN);
  spec.attach_type = probe.trace_point().type() == ir::shared::TracePoint::ENTRY
                         ? BPFProbeAttachType::kEntry
                         : BPFProbeAttachType::kReturn;
  spec.probe_fn = probe.name();

  if (binary_spec.language() == ir::shared::BinarySpec::GOLANG &&
      probe.trace_point().type() == ir::shared::TracePoint::RETURN) {
    return bpf_tools::TransformGolangReturnProbe(spec, elf_reader);
  }
  std::vector<UProbeSpec> specs = {spec};
  return specs;
}

StatusOr<BCCProgram::PerfBufferSpec> GetPerfBufferSpec(
    const absl::flat_hash_map<std::string_view, const ir::physical::Struct*>& structs,
    const ir::physical::PerfBufferOutput& output) {
  auto iter = structs.find(output.struct_type());

  if (iter == structs.end()) {
    return error::InvalidArgument("Struct '$0' was not defined", output.struct_type());
  }

  BCCProgram::PerfBufferSpec pf_spec;

  pf_spec.name = output.name();
  pf_spec.output = *iter->second;

  return pf_spec;
}

}  // namespace

StatusOr<BCCProgram> CompileProgram(const ir::logical::Program& input_program) {
  PL_ASSIGN_OR_RETURN(ir::logical::Program intermediate_program,
                      TransformLogicalProgram(input_program));

  pid_t pid = UProbeSpec::kDefaultPID;

  // Expect the outside caller keeps UPID, and specify them in UProbeSpec. Here upid and path are
  // specified alternatively, and upid will be replaced by binary path.
  switch (intermediate_program.binary_spec().target_oneof_case()) {
    case ir::shared::BinarySpec::TargetOneofCase::kPath:
      // Ignored, no need to resolve binary path.
      break;
    case ir::shared::BinarySpec::TargetOneofCase::kUpid: {
      pid = intermediate_program.binary_spec().upid().pid();

      PL_ASSIGN_OR_RETURN(std::filesystem::path binary_path, obj_tools::GetActiveBinary(pid));

      // TODO(yzhao): Here the upid's PID start time is not verified, just looks for the pid.
      // We might need to add such check in the future.

      intermediate_program.mutable_binary_spec()->set_path(binary_path.string());
      break;
    }
    case ir::shared::BinarySpec::TargetOneofCase::TARGET_ONEOF_NOT_SET:
      return error::InvalidArgument("Must specify path or upid");
  }

  LOG(INFO) << absl::Substitute("Tracepoint binary: $0", intermediate_program.binary_spec().path());

  PL_ASSIGN_OR_RETURN(ir::physical::Program physical_program, AddDwarves(intermediate_program));

  BCCProgram bcc_program;

  PL_ASSIGN_OR_RETURN(bcc_program.code, GenProgram(physical_program));

  std::unique_ptr<elf_tools::ElfReader> elf_reader;

  if (physical_program.binary_spec().language() == ir::shared::BinarySpec::GOLANG) {
    PL_ASSIGN_OR_RETURN(elf_reader,
                        elf_tools::ElfReader::Create(physical_program.binary_spec().path()));
  }

  for (const auto& probe : physical_program.probes()) {
    PL_ASSIGN_OR_RETURN(std::vector<UProbeSpec> specs,
                        GetUProbeSpec(physical_program.binary_spec(), probe, elf_reader.get()));
    for (auto& spec : specs) {
      bcc_program.uprobe_specs.push_back(std::move(spec));
    }
  }

  absl::flat_hash_map<std::string_view, const ir::physical::Struct*> structs;

  for (const auto& st : physical_program.structs()) {
    structs[st.name()] = &st;
  }

  for (const auto& output : physical_program.outputs()) {
    PL_ASSIGN_OR_RETURN(BCCProgram::PerfBufferSpec pf_spec, GetPerfBufferSpec(structs, output));
    bcc_program.perf_buffer_specs.push_back(std::move(pf_spec));
  }

  if (pid != UProbeSpec::kDefaultPID) {
    // TODO(yzhao): Seems with pid and binary both specified, detaching uprobes will fail after
    // the process exists. This does not happen if the pid was not specified.
    for (auto& spec : bcc_program.uprobe_specs) {
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
