#include "src/stirling/dynamic_tracing/dynamic_tracer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/system.h"

#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/dynamic_tracing/code_gen.h"
#include "src/stirling/dynamic_tracing/dwarvifier.h"
#include "src/stirling/dynamic_tracing/ir/sharedpb/shared.pb.h"
#include "src/stirling/dynamic_tracing/probe_transformer.h"
#include "src/stirling/obj_tools/dwarf_tools.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/obj_tools/proc_path_tools.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

using ::pl::stirling::bpf_tools::BPFProbeAttachType;
using ::pl::stirling::bpf_tools::UProbeSpec;

using ::pl::stirling::dwarf_tools::DwarfReader;
using ::pl::stirling::elf_tools::ElfReader;

namespace {

StatusOr<std::vector<UProbeSpec>> GetUProbeSpec(std::string_view binary_path,
                                                ir::shared::Language language,
                                                const ir::physical::Probe& probe,
                                                elf_tools::ElfReader* elf_reader) {
  UProbeSpec spec;

  spec.binary_path = binary_path;
  spec.symbol = probe.tracepoint().symbol();
  DCHECK(probe.tracepoint().type() == ir::shared::Tracepoint::ENTRY ||
         probe.tracepoint().type() == ir::shared::Tracepoint::RETURN);
  spec.attach_type = probe.tracepoint().type() == ir::shared::Tracepoint::ENTRY
                         ? BPFProbeAttachType::kEntry
                         : BPFProbeAttachType::kReturn;
  spec.probe_fn = probe.name();

  if (language == ir::shared::Language::GOLANG &&
      probe.tracepoint().type() == ir::shared::Tracepoint::RETURN) {
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

StatusOr<ir::shared::Language> TransformSourceLanguage(
    const llvm::dwarf::SourceLanguage& source_language) {
  switch (source_language) {
    case llvm::dwarf::DW_LANG_Go:
      return ir::shared::Language::GOLANG;
    case llvm::dwarf::DW_LANG_C:
    case llvm::dwarf::DW_LANG_C_plus_plus:
    case llvm::dwarf::DW_LANG_C_plus_plus_03:
    case llvm::dwarf::DW_LANG_C_plus_plus_11:
    case llvm::dwarf::DW_LANG_C_plus_plus_14:
      return ir::shared::Language::CPP;
    default:
      return error::Internal("Detected language $0 is not supported",
                             magic_enum::enum_name(source_language));
  }
}

void DetectSourceLanguage(ElfReader* elf_reader, DwarfReader* dwarf_reader,
                          ir::logical::TracepointDeployment* input_program) {
  // AUTO implies unknown, so use that to mean unknown.
  ir::shared::Language detected_language = ir::shared::Language::AUTO;

  // Primary detection mechanism is DWARF info, when available.
  if (dwarf_reader != nullptr) {
    detected_language = TransformSourceLanguage(dwarf_reader->source_language())
                            .ConsumeValueOr(ir::shared::Language::AUTO);
  } else {
    // Back-up detection policy looks for certain language-specific symbols
    if (elf_reader->SymbolAddress("runtime.buildVersion").has_value()) {
      detected_language = ir::shared::Language::GOLANG;
    }

    // TODO(oazizi): Make this stronger by adding more elf-based tests.
  }

  if (detected_language != ir::shared::Language::AUTO) {
    LOG(INFO) << absl::Substitute("Using language $0 for object $1",
                                  magic_enum::enum_name(dwarf_reader->source_language()),
                                  input_program->deployment_spec().path());

    // Since we only support tracing of a single object, all tracepoints have the same language.
    for (auto& tracepoint : *input_program->mutable_tracepoints()) {
      tracepoint.mutable_program()->set_language(detected_language);
    }
  } else {
    // For now, just print a warning, and let the probe proceed.
    // This is so we can use things like function argument tracing even when other features may not
    // work.
    LOG(WARNING) << absl::Substitute(
        "Language for object $0 is unknown or unsupported, so assuming C/C++ ABI. "
        "Some dynamic tracing features may not work, or may produce unexpected results.",
        input_program->deployment_spec().path());
  }
}

// Return value for Prepare(), so we can return multiple pointers.
struct ObjInfo {
  std::unique_ptr<ElfReader> elf_reader;
  std::unique_ptr<DwarfReader> dwarf_reader;
};

// Prepares the input program for compilation by:
// 1) Resolving the tracepoint target specification into an object path (e.g. UPID->path).
// 2) Preparing the Elf and Dwarf info for the binary.
// 3) Auto-detecting the source language.
StatusOr<ObjInfo> Prepare(ir::logical::TracepointDeployment* input_program) {
  ObjInfo obj_info;

  // Note that this should do nothing when called as part of Stirling as a whole,
  // because it was already called by RegisterTracepoint() in stirling.
  // The extra call should turn into a NOP.
  PL_RETURN_IF_ERROR(ResolveTargetObjPath(input_program->mutable_deployment_spec()));

  const auto& binary_path = input_program->deployment_spec().path();
  LOG(INFO) << absl::Substitute("Tracepoint binary: $0", binary_path);

  PL_ASSIGN_OR_RETURN(obj_info.elf_reader, ElfReader::Create(binary_path));

  const auto& debug_symbols_path = obj_info.elf_reader->debug_symbols_path().string();

  obj_info.dwarf_reader = DwarfReader::Create(debug_symbols_path).ConsumeValueOr(nullptr);

  DetectSourceLanguage(obj_info.elf_reader.get(), obj_info.dwarf_reader.get(), input_program);

  return obj_info;
}

}  // namespace

StatusOr<BCCProgram> CompileProgram(ir::logical::TracepointDeployment* input_program) {
  if (input_program->tracepoints_size() != 1) {
    return error::InvalidArgument("Only one tracepoint currently supported, got '$0'",
                                  input_program->tracepoints_size());
  }

  // --------------------------
  // Pre-processing
  // --------------------------

  // Prepares the input program by making adjustments to the input program,
  // and also returning the ELF and DWARF readers for the program.
  PL_ASSIGN_OR_RETURN(ObjInfo obj_info, Prepare(input_program));

  // --------------------------
  // Main compilation pipeline
  // --------------------------

  PL_ASSIGN_OR_RETURN(ir::logical::TracepointDeployment intermediate_program,
                      TransformLogicalProgram(*input_program));

  PL_ASSIGN_OR_RETURN(ir::physical::Program physical_program,
                      GeneratePhysicalProgram(intermediate_program, obj_info.dwarf_reader.get()));

  PL_ASSIGN_OR_RETURN(std::string bcc_code, GenBCCProgram(physical_program));

  // --------------------------
  // Generate BCC Program Object
  // --------------------------

  // TODO(oazizi): Move the code below into its own function.

  BCCProgram bcc_program;
  bcc_program.code = std::move(bcc_code);

  pid_t pid = physical_program.deployment_spec().upid().pid();
  const ir::shared::Language& language = physical_program.language();
  const std::string& binary_path = physical_program.deployment_spec().path();

  for (const auto& probe : physical_program.probes()) {
    PL_ASSIGN_OR_RETURN(std::vector<UProbeSpec> specs,
                        GetUProbeSpec(binary_path, language, probe, obj_info.elf_reader.get()));
    for (auto& spec : specs) {
      // When PID is specified, the binary_path must still be provided. The PID only further filters
      // which instances of the binary_path will be traced.
      // TODO(yzhao): Seems with pid and binary both specified, detaching uprobes will fail after
      // the process exists. This does not happen if the pid was not specified.
      if (pid != 0) {
        spec.pid = pid;
      }

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

  return bcc_program;
}

namespace {

StatusOr<std::filesystem::path> ResolveUPID(const ir::shared::DeploymentSpec& deployment_spec) {
  uint32_t pid = deployment_spec.upid().pid();

  std::optional<int64_t> start_time;
  if (deployment_spec.upid().ts_ns() != 0) {
    start_time = deployment_spec.upid().ts_ns();
  }

  return obj_tools::GetPIDBinaryOnHost(pid, start_time);
}

StatusOr<std::filesystem::path> ResolveSharedObject(
    const ir::shared::DeploymentSpec& deployment_spec) {
  const uint32_t& pid = deployment_spec.shared_object().upid().pid();
  const std::string& lib_name = deployment_spec.shared_object().name();

  std::filesystem::path proc_pid_path =
      system::Config::GetInstance().proc_path() / std::to_string(pid);
  if (deployment_spec.upid().ts_ns() != 0) {
    int64_t spec_start_time = deployment_spec.upid().ts_ns();

    PL_ASSIGN_OR_RETURN(int64_t pid_start_time, system::GetPIDStartTimeTicks(proc_pid_path));
    if (spec_start_time != pid_start_time) {
      return error::NotFound(
          "This is not the pid you are looking for... "
          "Start time does not match (specification: $0 vs system: $1).",
          spec_start_time, pid_start_time);
    }
  }

  // Find the path to shared library, which may be inside a container.
  system::ProcParser proc_parser(system::Config::GetInstance());
  PL_ASSIGN_OR_RETURN(absl::flat_hash_set<std::string> libs_status, proc_parser.GetMapPaths(pid));

  const std::filesystem::path& host_path = system::Config::GetInstance().host_path();
  for (const auto& lib : libs_status) {
    // Look for a library name such as /lib/libc.so.6 or /lib/libc-2.32.so.
    // The name is assumed to end with either a '.' or a '-'.
    std::string lib_path_filename = std::filesystem::path(lib).filename().string();
    if (absl::StartsWith(lib_path_filename, absl::StrCat(lib_name, ".")) ||
        absl::StartsWith(lib_path_filename, absl::StrCat(lib_name, "-"))) {
      PL_ASSIGN_OR_RETURN(std::filesystem::path lib_path,
                          obj_tools::ResolveProcessPath(proc_pid_path, lib));

      // If we're running in a container, convert exe to be relative to our host mount.
      // Note that we mount host '/' to '/host' inside container.
      // Warning: must use JoinPath, because we are dealing with two absolute paths.
      return fs::JoinPath({&host_path, &lib_path});
    }
  }

  return error::Internal("Could not find shared library $0 in context of PID $1.", lib_name, pid);
}

}  // namespace

Status ResolveTargetObjPath(ir::shared::DeploymentSpec* deployment_spec) {
  std::filesystem::path target_obj_path;

  switch (deployment_spec->target_oneof_case()) {
    // Already a path, so nothing to do.
    case ir::shared::DeploymentSpec::TargetOneofCase::kPath:
      target_obj_path = deployment_spec->path();
      break;
    // Populate path based on UPID.
    case ir::shared::DeploymentSpec::TargetOneofCase::kUpid: {
      PL_ASSIGN_OR_RETURN(target_obj_path, ResolveUPID(*deployment_spec));
      break;
    }
    // Populate path based on shared object identifier.
    case ir::shared::DeploymentSpec::TargetOneofCase::kSharedObject: {
      PL_ASSIGN_OR_RETURN(target_obj_path, ResolveSharedObject(*deployment_spec));
      break;
    }
    case ir::shared::DeploymentSpec::TargetOneofCase::TARGET_ONEOF_NOT_SET:
      return error::InvalidArgument("Must specify target.");
  }

  if (!fs::Exists(target_obj_path).ok()) {
    return error::Internal("Binary $0 not found.", target_obj_path.string());
  }

  deployment_spec->set_path(target_obj_path.string());

  return Status::OK();
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
