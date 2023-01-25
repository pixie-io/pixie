/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"

#include <algorithm>
#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/system/system.h"

#include "src/shared/metadata/k8s_objects.h"
#include "src/shared/upid/upid.h"

#include "src/stirling/bpf_tools/utils.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/elf_reader.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/autogen.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/code_gen.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dwarvifier.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/sharedpb/shared.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/probe_transformer.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_bool(debug_dt_pipeline, false, "Enable logging of the Dynamic Tracing pipeline IR graphs.");

namespace px {
namespace stirling {
namespace dynamic_tracing {

using ::px::stirling::bpf_tools::BPFProbeAttachType;
using ::px::stirling::bpf_tools::UProbeSpec;
using ::px::stirling::obj_tools::DwarfReader;
using ::px::stirling::obj_tools::ElfReader;
using ::px::system::ProcParser;
using ::px::system::ProcPidRootPath;

namespace {

StatusOr<std::vector<UProbeSpec>> GetUProbeSpec(std::string_view binary_path,
                                                ir::shared::Language language,
                                                const ir::physical::Probe& probe,
                                                obj_tools::ElfReader* elf_reader) {
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

// Return value for Prepare(), so we can return multiple pointers.
struct ObjInfo {
  std::unique_ptr<ElfReader> elf_reader;
  std::unique_ptr<DwarfReader> dwarf_reader;
};

// Prepares the input program for compilation by:
// 1) Resolving the tracepoint target specification into an object path (e.g. UPID->path).
// 2) Preparing the Elf and Dwarf info for the binary.
StatusOr<ObjInfo> Prepare(const ir::logical::TracepointDeployment& input_program) {
  ObjInfo obj_info;

  const auto& binary_path = input_program.deployment_spec().path_list().paths(0);
  LOG(INFO) << absl::Substitute("Tracepoint binary: $0", binary_path);

  PX_ASSIGN_OR_RETURN(obj_info.elf_reader, ElfReader::Create(binary_path));

  const auto& debug_symbols_path = obj_info.elf_reader->debug_symbols_path().string();

  obj_info.dwarf_reader =
      DwarfReader::CreateIndexingAll(debug_symbols_path).ConsumeValueOr(nullptr);

  return obj_info;
}

}  // namespace

StatusOr<BCCProgram> CompileProgram(ir::logical::TracepointDeployment* input_program) {
  if (input_program->deployment_spec().path_list().paths_size() == 0) {
    return error::InvalidArgument("Must have path resolved before compiling program");
  }

  if (input_program->tracepoints_size() != 1) {
    return error::InvalidArgument("Only one tracepoint currently supported, got '$0'",
                                  input_program->tracepoints_size());
  }

  // Get the ELF and DWARF readers for the program.
  PX_ASSIGN_OR_RETURN(ObjInfo obj_info, Prepare(*input_program));

  // --------------------------
  // Pre-processing pipeline
  // --------------------------

  // Populate source language.
  DetectSourceLanguage(obj_info.elf_reader.get(), obj_info.dwarf_reader.get(), input_program);

  // Expand symbol.
  PX_RETURN_IF_ERROR(ResolveProbeSymbol(obj_info.elf_reader.get(), input_program));

  LOG_IF(INFO, FLAGS_debug_dt_pipeline) << input_program->DebugString();

  // Auto-gen probe variables
  PX_RETURN_IF_ERROR(AutoTraceExpansion(obj_info.dwarf_reader.get(), input_program));

  LOG_IF(INFO, FLAGS_debug_dt_pipeline) << input_program->DebugString();

  // --------------------------
  // Main compilation pipeline
  // --------------------------

  PX_ASSIGN_OR_RETURN(ir::logical::TracepointDeployment intermediate_program,
                      TransformLogicalProgram(*input_program));

  LOG_IF(INFO, FLAGS_debug_dt_pipeline) << input_program->DebugString();

  PX_ASSIGN_OR_RETURN(ir::physical::Program physical_program,
                      GeneratePhysicalProgram(intermediate_program, obj_info.dwarf_reader.get(),
                                              obj_info.elf_reader.get()));

  LOG_IF(INFO, FLAGS_debug_dt_pipeline) << physical_program.DebugString();

  PX_ASSIGN_OR_RETURN(std::string bcc_code, GenBCCProgram(physical_program));

  // --------------------------
  // Generate BCC Program Object
  // --------------------------

  // TODO(oazizi): Move the code below into its own function.

  BCCProgram bcc_program;
  bcc_program.code = std::move(bcc_code);

  const ir::shared::Language& language = physical_program.language();
  const std::string& binary_path = physical_program.deployment_spec().path_list().paths(0);

  // TODO(yzhao): deployment_spec.upid will be lost after calling ResolveTargetObjPath().
  // Consider adjust data structure such that both can be preserved.

  for (const auto& probe : physical_program.probes()) {
    PX_ASSIGN_OR_RETURN(std::vector<UProbeSpec> specs,
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
    PX_ASSIGN_OR_RETURN(BCCProgram::PerfBufferSpec pf_spec, GetPerfBufferSpec(structs, output));
    bcc_program.perf_buffer_specs.push_back(std::move(pf_spec));
  }

  return bcc_program;
}

namespace {

Status CheckPIDStartTime(const ProcParser& proc_parser, int32_t pid, int64_t spec_start_time) {
  PX_ASSIGN_OR_RETURN(int64_t pid_start_time, proc_parser.GetPIDStartTimeTicks(pid));
  if (spec_start_time != pid_start_time) {
    return error::NotFound(
        "This is not the pid you are looking for... "
        "Start time does not match (specification: $0 vs system: $1).",
        spec_start_time, pid_start_time);
  }
  return Status::OK();
}

StatusOr<std::filesystem::path> ResolveUPID(const ir::shared::UPID& upid) {
  const uint32_t pid = upid.pid();
  const ProcParser proc_parser;

  if (upid.ts_ns() != 0) {
    PX_RETURN_IF_ERROR(CheckPIDStartTime(proc_parser, pid, upid.ts_ns()));
  }

  PX_ASSIGN_OR_RETURN(const std::filesystem::path proc_exe, proc_parser.GetExePath(pid));
  const auto host_proc_exe = ProcPidRootPath(pid, proc_exe);

  if (!fs::Exists(host_proc_exe)) {
    return error::Internal("Binary not found: $0.", host_proc_exe.string());
  }
  return host_proc_exe;
}

StatusOr<std::filesystem::path> ResolveSharedObject(
    const ir::shared::DeploymentSpec& deployment_spec) {
  const uint32_t& pid = deployment_spec.shared_object().upid().pid();
  const std::string& lib_name = deployment_spec.shared_object().name();
  const ProcParser proc_parser;
  auto ts_ns = deployment_spec.shared_object().upid().ts_ns();
  if (ts_ns != 0) {
    PX_RETURN_IF_ERROR(CheckPIDStartTime(proc_parser, pid, ts_ns));
  }

  // Find the path to shared library, which may be inside a container.
  PX_ASSIGN_OR_RETURN(absl::flat_hash_set<std::string> libs_status, proc_parser.GetMapPaths(pid));

  for (const auto& lib : libs_status) {
    // Look for a library name such as /lib/libc.so.6 or /lib/libc-2.32.so.
    // The name is assumed to end with either a '.' or a '-'.
    std::string lib_path_filename = std::filesystem::path(lib).filename().string();
    if (absl::StartsWith(lib_path_filename, absl::StrCat(lib_name, ".")) ||
        absl::StartsWith(lib_path_filename, absl::StrCat(lib_name, "-"))) {
      const auto lib_path = ProcPidRootPath(pid, lib);
      if (!fs::Exists(lib_path)) {
        return error::Internal("Lib path not found: $0.", lib_path.string());
      }
      return lib_path;
    }
  }

  return error::Internal("Shared library not found: $0, PID: $1.", lib_name, pid);
}

using K8sNameIdentView = ::px::md::K8sMetadataState::K8sNameIdentView;

// pod_name is formatted as <namespace>/<name>.
StatusOr<K8sNameIdentView> GetPodNameIdent(std::string_view pod_name) {
  std::vector<std::string_view> ns_and_name = absl::StrSplit(pod_name, '/');

  if (ns_and_name.size() != 2) {
    return error::InvalidArgument("Invalid Pod name, expect '<namespace>/<name>', got '$0'",
                                  pod_name);
  }

  return K8sNameIdentView(ns_and_name.front(), ns_and_name.back());
}

// Returns a protobuf message from the corresponding native object.
ir::shared::UPID UPIDToProto(const md::UPID& upid) {
  dynamic_tracing::ir::shared::UPID res;
  res.set_asid(upid.asid());
  res.set_pid(upid.pid());
  res.set_ts_ns(upid.start_ts());
  return res;
}

StatusOr<const md::PodInfo*> ResolvePod(const md::K8sMetadataState& k8s_mds,
                                        std::string_view pod_name) {
  PX_ASSIGN_OR_RETURN(K8sNameIdentView name_ident_view, GetPodNameIdent(pod_name));

  std::vector<std::string> pod_names;
  std::vector<const md::PodInfo*> pod_infos;

  for (const auto& [name_ident, uid] : k8s_mds.pods_by_name()) {
    if (name_ident.first != name_ident_view.first) {
      continue;
    }
    if (!absl::StartsWith(name_ident.second, name_ident_view.second)) {
      continue;
    }
    const auto* pod_info = k8s_mds.PodInfoByID(uid);
    if (pod_info == nullptr) {
      return error::Internal("Pod name '$0' is recognized, but PodInfo is not found", pod_name);
    }
    if (pod_info->stop_time_ns() > 0) {
      return error::NotFound("Pod '$0' has died", pod_name);
    }
    pod_names.push_back(absl::StrCat(name_ident.first, "/", name_ident.second));
    pod_infos.push_back(pod_info);
  }

  if (pod_names.empty()) {
    return error::NotFound("Could not find Pod for name '$0'", pod_name);
  }

  if (pod_names.size() > 1) {
    return error::FailedPrecondition("Pod name '$0' matches multiple Pods: '$1'", pod_name,
                                     absl::StrJoin(pod_names, ","));
  }

  return pod_infos.front();
}

StatusOr<const md::ContainerInfo*> ResolveContainer(const md::K8sMetadataState& k8s_mds,
                                                    const md::PodInfo& pod_info,
                                                    std::string_view container_name) {
  absl::flat_hash_map<std::string_view, const md::ContainerInfo*> name_to_container_info;
  std::vector<std::string_view> container_names;

  for (const auto& container_id : pod_info.containers()) {
    auto* container_info = k8s_mds.ContainerInfoByID(container_id);
    if (container_info == nullptr || container_info->stop_time_ns() > 0) {
      continue;
    }
    name_to_container_info[container_info->name()] = container_info;
    container_names.push_back(container_info->name());
  }

  if (name_to_container_info.empty()) {
    return error::FailedPrecondition("There is no live container in Pod '$0'", pod_info.name());
  }

  if (name_to_container_info.size() > 1 && container_name.empty()) {
    std::sort(container_names.begin(), container_names.end());
    return error::FailedPrecondition(
        "Container name not specified, but Pod '$0' has multiple containers '$1'", pod_info.name(),
        absl::StrJoin(container_names, ","));
  }

  const md::ContainerInfo* container_info = nullptr;

  if (container_name.empty()) {
    DCHECK_EQ(name_to_container_info.size(), 1ul);
    container_info = name_to_container_info.begin()->second;
  } else {
    auto iter = name_to_container_info.find(container_name);
    if (iter == name_to_container_info.end()) {
      return error::NotFound("Could not find live container '$0' in Pod: '$1'", container_name,
                             pod_info.name());
    }
    container_info = iter->second;
  }

  return container_info;
}

StatusOr<md::UPID> ResolveProcess(const md::ContainerInfo& container_info,
                                  std::string_view process_regexp) {
  if (container_info.active_upids().size() > 1 && process_regexp.empty()) {
    // TODO(yzhao): Consider resolve UPID's command line, so that we can include them in the error
    // message, which helps users to update their pxtrace.PodProcess().
    return error::FailedPrecondition(
        "Process name regexp not specified, but Container '$0' has multiple processes",
        container_info.name());
  }

  std::vector<md::UPID> upids;
  system::ProcParser proc_parser;

  for (const auto& upid : container_info.active_upids()) {
    if (!process_regexp.empty()) {
      std::string cmd = proc_parser.GetPIDCmdline(upid.pid());
      std::smatch match_results;
      std::regex regex(process_regexp.data(), process_regexp.size());
      if (!std::regex_search(cmd, match_results, regex, std::regex_constants::match_any)) {
        continue;
      }
    }
    upids.push_back(upid);
  }

  if (upids.empty()) {
    return error::NotFound("Found no UPIDs in Container: '$0'", container_info.name());
  }
  if (upids.size() > 1) {
    // TODO(yzhao): Consider resolve UPID's command line, so that we can include them in the error
    // message, which helps users to update their pxtrace.PodProcess().
    return error::Internal("Found more than 1 UPIDs for Container: '$0'", container_info.name());
  }

  return upids.front();
}

#define PX_ASSIGN_OR_CONTINUE(lhs, rexpr, error_msgs) \
  PX_ASSIGN_OR(lhs, rexpr, error_msgs.push_back(__s__.msg()); continue;)

// Given a TracepointDeployment that specifies a Pod as the target, resolves the UPIDs, and writes
// them into the input protobuf.
Status ResolvePodProcess(const md::K8sMetadataState& k8s_mds,
                         dynamic_tracing::ir::shared::DeploymentSpec* deployment_spec) {
  // Copy pod_process before setting deployment_spec to upid.
  auto pod_process = deployment_spec->pod_process();
  std::string_view container_name = pod_process.container();
  std::string_view process_regexp = pod_process.process();

  auto upid_list = deployment_spec->mutable_upid_list();
  int pods_size = pod_process.pods_size();

  if (pods_size == 0) {
    return error::NotFound("No pods are provided in PodProcess.");
  }

  std::vector<std::string> error_msgs;
  for (int i = 0; i < pods_size; ++i) {
    std::string_view pod_name(pod_process.pods(i));
    // If a pod doesn't exist, then try other pods.
    PX_ASSIGN_OR_CONTINUE(const md::PodInfo* pod_info, ResolvePod(k8s_mds, pod_name), error_msgs);
    PX_ASSIGN_OR_CONTINUE(const md::ContainerInfo* container_info,
                          ResolveContainer(k8s_mds, *pod_info, container_name), error_msgs);
    PX_ASSIGN_OR_CONTINUE(const md::UPID upid, ResolveProcess(*container_info, process_regexp),
                          error_msgs);

    auto upid_ptr = upid_list->add_upids();
    upid_ptr->CopyFrom(UPIDToProto(upid));
  }

  if (upid_list->upids_size() == 0) {
    return error::FailedPrecondition(absl::StrJoin(error_msgs, "\n"));
  }

  return Status::OK();
}

}  // namespace

Status ResolveTargetObjPaths(const md::K8sMetadataState& k8s_mds,
                             ir::shared::DeploymentSpec* deployment_spec) {
  // Write PodProcess to deployment_spec.upid.
  if (deployment_spec->has_pod_process()) {
    PX_RETURN_IF_ERROR(ResolvePodProcess(k8s_mds, deployment_spec));
  }

  std::filesystem::path target_obj_path;

  // TODO(chengruizhe/oazizi): Consider removing switch statement, and changes into a sequential
  // processing workflow: Pod->UPID->Path.
  switch (deployment_spec->target_oneof_case()) {
    // Already paths, so nothing to do.
    case ir::shared::DeploymentSpec::TargetOneofCase::kPathList:
      break;
    // Populate paths based on UPIDs.
    case ir::shared::DeploymentSpec::TargetOneofCase::kUpidList: {
      const int upids_size = deployment_spec->upid_list().upids_size();
      // Copy upid_list.
      const auto upid_list = deployment_spec->upid_list();

      absl::flat_hash_set<std::string> inserted_paths;
      for (int i = 0; i < upids_size; ++i) {
        PX_ASSIGN_OR_RETURN(target_obj_path, ResolveUPID(upid_list.upids(i)));
        // Deduplicate identical target paths before adding to deployment_spec.
        if (inserted_paths.find(target_obj_path.string()) == inserted_paths.end()) {
          inserted_paths.insert(target_obj_path.string());
          deployment_spec->mutable_path_list()->add_paths(target_obj_path);
        }
      }
      break;
    }
    // Populate paths based on shared object identifier.
    case ir::shared::DeploymentSpec::TargetOneofCase::kSharedObject: {
      PX_ASSIGN_OR_RETURN(target_obj_path, ResolveSharedObject(*deployment_spec));
      deployment_spec->mutable_path_list()->add_paths(target_obj_path);
      break;
    }
    case ir::shared::DeploymentSpec::TargetOneofCase::kPodProcess: {
      LOG(DFATAL) << "This should never happen, pod process must have been rewritten to UPID.";
      break;
    }
    case ir::shared::DeploymentSpec::TargetOneofCase::TARGET_ONEOF_NOT_SET:
      return error::InvalidArgument("Must specify target.");
  }

  for (auto& target_obj_path : deployment_spec->path_list().paths()) {
    if (!fs::Exists(target_obj_path)) {
      return error::Internal("Binary $0 not found.", target_obj_path);
    }
  }
  return Status::OK();
}

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
