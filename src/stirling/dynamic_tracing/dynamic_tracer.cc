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
  ir::shared::Language detected_language = ir::shared::Language::LANG_UNKNOWN;

  // Primary detection mechanism is DWARF info, when available.
  if (dwarf_reader != nullptr) {
    detected_language = TransformSourceLanguage(dwarf_reader->source_language())
                            .ConsumeValueOr(ir::shared::Language::LANG_UNKNOWN);
  } else {
    // Back-up detection policy looks for certain language-specific symbols
    if (elf_reader->SymbolAddress("runtime.buildVersion").has_value()) {
      detected_language = ir::shared::Language::GOLANG;
    }

    // TODO(oazizi): Make this stronger by adding more elf-based tests.
  }

  if (detected_language != ir::shared::Language::LANG_UNKNOWN) {
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
  if (input_program->deployment_spec().path().empty()) {
    return error::InvalidArgument("Must have path resolved before compiling program");
  }

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
                      GeneratePhysicalProgram(intermediate_program, obj_info.dwarf_reader.get(),
                                              obj_info.elf_reader.get()));

  PL_ASSIGN_OR_RETURN(std::string bcc_code, GenBCCProgram(physical_program));

  // --------------------------
  // Generate BCC Program Object
  // --------------------------

  // TODO(oazizi): Move the code below into its own function.

  BCCProgram bcc_program;
  bcc_program.code = std::move(bcc_code);

  const ir::shared::Language& language = physical_program.language();
  const std::string& binary_path = physical_program.deployment_spec().path();

  // TODO(yzhao): deployment_spec.upid will be lost after calling ResolveTargetObjPath().
  // Consider adjust data structure such that both can be preserved.

  for (const auto& probe : physical_program.probes()) {
    PL_ASSIGN_OR_RETURN(std::vector<UProbeSpec> specs,
                        GetUProbeSpec(binary_path, language, probe, obj_info.elf_reader.get()));
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

  return bcc_program;
}

namespace {

StatusOr<std::filesystem::path> ResolveUPID(const ir::shared::DeploymentSpec& deployment_spec) {
  uint32_t pid = deployment_spec.upid().pid();

  std::optional<int64_t> start_time;
  if (deployment_spec.upid().ts_ns() != 0) {
    start_time = deployment_spec.upid().ts_ns();
  }

  PL_ASSIGN_OR_RETURN(std::filesystem::path pid_binary,
                      obj_tools::ResolvePIDBinary(pid, start_time));
  pid_binary = system::Config::GetInstance().ToHostPath(pid_binary);
  PL_RETURN_IF_ERROR(fs::Exists(pid_binary));
  return pid_binary;
}

StatusOr<std::filesystem::path> ResolveSharedObject(
    const ir::shared::DeploymentSpec& deployment_spec) {
  const uint32_t& pid = deployment_spec.shared_object().upid().pid();
  const std::string& lib_name = deployment_spec.shared_object().name();

  const system::Config& sysconfig = system::Config::GetInstance();

  std::filesystem::path proc_pid_path = sysconfig.proc_path() / std::to_string(pid);
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
  system::ProcParser proc_parser(sysconfig);
  PL_ASSIGN_OR_RETURN(absl::flat_hash_set<std::string> libs_status, proc_parser.GetMapPaths(pid));

  for (const auto& lib : libs_status) {
    // Look for a library name such as /lib/libc.so.6 or /lib/libc-2.32.so.
    // The name is assumed to end with either a '.' or a '-'.
    std::string lib_path_filename = std::filesystem::path(lib).filename().string();
    if (absl::StartsWith(lib_path_filename, absl::StrCat(lib_name, ".")) ||
        absl::StartsWith(lib_path_filename, absl::StrCat(lib_name, "-"))) {
      PL_ASSIGN_OR_RETURN(std::filesystem::path lib_path,
                          obj_tools::ResolveProcessPath(proc_pid_path, lib));
      lib_path = sysconfig.ToHostPath(lib_path);
      PL_RETURN_IF_ERROR(fs::Exists(lib_path));
      return lib_path;
    }
  }

  return error::Internal("Could not find shared library $0 in context of PID $1.", lib_name, pid);
}

using K8sNameIdentView = ::pl::md::K8sMetadataState::K8sNameIdentView;

// pod_name is formatted as <namespace>/<name>.
StatusOr<K8sNameIdentView> GetPodNameIdent(std::string_view pod_name) {
  std::vector<std::string_view> ns_and_name = absl::StrSplit(pod_name, '/');

  if (ns_and_name.size() != 2) {
    return error::InvalidArgument("Invalid Pod name, expect '<namespace>/<name>', got '$0'",
                                  pod_name);
  }

  return K8sNameIdentView(ns_and_name.front(), ns_and_name.back());
}

// Returns a list of UPIDs belong to the Pod with the specified name.
StatusOr<std::vector<md::UPID>> GetPodUPIDs(const md::K8sMetadataState& k8s_mds,
                                            std::string_view pod_name) {
  PL_ASSIGN_OR_RETURN(K8sNameIdentView name_ident_view, GetPodNameIdent(pod_name));

  std::string pod_uid = k8s_mds.PodIDByName(name_ident_view);
  if (pod_uid.empty()) {
    return error::InvalidArgument("Could not find Pod for name '$0'", pod_name);
  }

  auto* pod_info = k8s_mds.PodInfoByID(pod_uid);
  if (pod_info == nullptr) {
    return error::InvalidArgument("Pod '$0' is recognized, but PodInfo is not found", pod_name);
  }
  if (pod_info->stop_time_ns() > 0) {
    return error::InvalidArgument("Pod '$0' has died", pod_name);
  }

  std::vector<md::UPID> res;
  for (const auto& container_id : pod_info->containers()) {
    auto* container_info = k8s_mds.ContainerInfoByID(container_id);
    if (container_info == nullptr || container_info->stop_time_ns() > 0) {
      continue;
    }
    for (const auto& upid : container_info->active_upids()) {
      res.push_back(upid);
    }
  }
  return res;
}

// Returns a protobuf message from the corresponding native object.
ir::shared::UPID UPIDToProto(const md::UPID& upid) {
  dynamic_tracing::ir::shared::UPID res;
  res.set_asid(upid.asid());
  res.set_pid(upid.pid());
  res.set_ts_ns(upid.start_ts());
  return res;
}

// Given a TracepointDeployment that specifies a Pod as the target, resolves the UPIDs, and writes
// them into the input protobuf.
Status ResolvePodUPIDs(const md::K8sMetadataState& k8s_mds,
                       dynamic_tracing::ir::shared::DeploymentSpec* deployment_spec) {
  std::string_view pod_name = deployment_spec->pod();

  PL_ASSIGN_OR_RETURN(std::vector<md::UPID> pod_upids, GetPodUPIDs(k8s_mds, pod_name));

  if (pod_upids.empty()) {
    return error::NotFound("Found no UPIDs for Pod '$0'", pod_name);
  }
  if (pod_upids.size() > 1) {
    return error::Internal("Found more than 1 UPIDs for Pod '$0'", pod_name);
  }
  // Target oneof now clears the pod.
  deployment_spec->mutable_upid()->CopyFrom(UPIDToProto(pod_upids.front()));

  return Status::OK();
}

}  // namespace

Status ResolveTargetObjPath(const md::K8sMetadataState& k8s_mds,
                            ir::shared::DeploymentSpec* deployment_spec) {
  // Write Pod UPID to deployment_spec.upid.
  if (!deployment_spec->pod().empty()) {
    PL_RETURN_IF_ERROR(ResolvePodUPIDs(k8s_mds, deployment_spec));
  }

  std::filesystem::path target_obj_path;

  // TODO(yzhao/oazizi): Consider removing switch statement, and changes into a sequential
  // processing workflow: Pod->UPID->Path.
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
    // Populate UPIDs based on Pod name.
    case ir::shared::DeploymentSpec::TargetOneofCase::kPod: {
      LOG(DFATAL) << "This should never happen, pod must have been rewritten to UPID.";
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
