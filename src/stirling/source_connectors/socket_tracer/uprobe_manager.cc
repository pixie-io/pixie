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

#include "src/stirling/source_connectors/socket_tracer/uprobe_manager.h"

#include <fcntl.h>
#include <openssl/md5.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <tuple>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_pid_path.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/obj_tools/dwarf_reader.h"
#include "src/stirling/obj_tools/go_syms.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"
#include "src/stirling/utils/linux_headers.h"
#include "src/stirling/utils/proc_path_tools.h"

DEFINE_bool(stirling_rescan_for_dlopen, false,
            "If enabled, Stirling will use mmap tracing information to rescan binaries for delay "
            "loaded libraries like OpenSSL");
DEFINE_bool(stirling_enable_grpc_c_tracing,
            gflags::BoolFromEnv("STIRLING_ENABLE_GRPC_C_TRACING", false),
            "If true, enable gRPC tracing for C dynamic libraries used for python");
DEFINE_double(stirling_rescan_exp_backoff_factor, 2.0,
              "Exponential backoff factor used in decided how often to rescan binaries for "
              "dynamically loaded libraries");

namespace px {
namespace stirling {

using ::px::stirling::obj_tools::DwarfReader;
using ::px::stirling::obj_tools::ElfReader;
using ::px::stirling::utils::GetKernelVersion;
using ::px::stirling::utils::KernelVersion;
using ::px::stirling::utils::KernelVersionOrder;
using ::px::system::ProcPidRootPath;

UProbeManager::UProbeManager(bpf_tools::BCCWrapper* bcc) : bcc_(bcc) {
  proc_parser_ = std::make_unique<system::ProcParser>();
}

void UProbeManager::Init(bool disable_go_tls_tracing, bool enable_http2_tracing,
                         bool disable_self_probing) {
  cfg_disable_go_tls_tracing_ = disable_go_tls_tracing;
  cfg_enable_http2_tracing_ = enable_http2_tracing;
  cfg_disable_self_probing_ = disable_self_probing;

  openssl_source_map_ = WrappedBCCMap<uint32_t, ssl_source_t>::Create(bcc_, "openssl_source_map");
  openssl_symaddrs_map_ =
      WrappedBCCMap<uint32_t, struct openssl_symaddrs_t>::Create(bcc_, "openssl_symaddrs_map");
  go_common_symaddrs_map_ =
      WrappedBCCMap<uint32_t, struct go_common_symaddrs_t>::Create(bcc_, "go_common_symaddrs_map");
  go_http2_symaddrs_map_ =
      WrappedBCCMap<uint32_t, struct go_http2_symaddrs_t>::Create(bcc_, "http2_symaddrs_map");
  go_tls_symaddrs_map_ =
      WrappedBCCMap<uint32_t, struct go_tls_symaddrs_t>::Create(bcc_, "go_tls_symaddrs_map");
  node_tlswrap_symaddrs_map_ = WrappedBCCMap<uint32_t, struct node_tlswrap_symaddrs_t>::Create(
      bcc_, "node_tlswrap_symaddrs_map");
  grpc_c_versions_map_ = WrappedBCCMap<uint32_t, uint64_t>::Create(bcc_, "grpc_c_versions");
}

void UProbeManager::NotifyMMapEvent(upid_t upid) {
  if (FLAGS_stirling_rescan_for_dlopen) {
    upids_with_mmap_.insert(upid);
  }
}

Status UProbeManager::LogAndAttachUProbe(const bpf_tools::UProbeSpec& spec) {
  auto s = bcc_->AttachUProbe(spec);
  if (!s.ok()) {
    monitor_.AppendProbeStatusRecord("socket_tracer", spec.probe_fn, s, spec.ToJSON());
  }
  return s;
}

StatusOr<int> UProbeManager::AttachUProbeTmpl(const ArrayView<UProbeTmpl>& probe_tmpls,
                                              const std::string& binary,
                                              obj_tools::ElfReader* elf_reader) {
  using bpf_tools::BPFProbeAttachType;

  int uprobe_count = 0;
  for (const auto& tmpl : probe_tmpls) {
    bpf_tools::UProbeSpec spec = {binary,
                                  /*symbol*/ {},
                                  /*address*/ 0,    bpf_tools::UProbeSpec::kDefaultPID,
                                  tmpl.attach_type, std::string(tmpl.probe_fn)};

    StatusOr<std::vector<ElfReader::SymbolInfo>> symbol_infos_status =
        elf_reader->ListFuncSymbols(tmpl.symbol, tmpl.match_type);
    if (!symbol_infos_status.ok()) {
      VLOG(1) << absl::Substitute("Could not list symbols [error=$0]",
                                  symbol_infos_status.ToString());
      continue;
    }
    const std::vector<ElfReader::SymbolInfo>& symbol_infos = symbol_infos_status.ValueOrDie();

    for (const auto& symbol_info : symbol_infos) {
      switch (tmpl.attach_type) {
        case BPFProbeAttachType::kEntry:
        case BPFProbeAttachType::kReturn: {
          spec.symbol = symbol_info.name;
          PX_RETURN_IF_ERROR(LogAndAttachUProbe(spec));
          ++uprobe_count;
          break;
        }
        case BPFProbeAttachType::kReturnInsts: {
          // TODO(yzhao): The following code that produces multiple UProbeSpec objects cannot be
          // replaced by TransformGolangReturnProbe(), because LLVM and ELFIO defines conflicting
          // symbol: EI_MAG0 appears as enum in include/llvm/BinaryFormat/ELF.h [1] and
          // EI_MAG0 appears as a macro in elfio/elf_types.hpp [2]. And there are many other such
          // symbols as well.
          //
          // [1] https://llvm.org/doxygen/BinaryFormat_2ELF_8h_source.html
          // [2] https://github.com/eth-sri/debin/blob/master/cpp/elfio/elf_types.hpp
          PX_ASSIGN_OR_RETURN(std::vector<uint64_t> ret_inst_addrs,
                              elf_reader->FuncRetInstAddrs(symbol_info));
          for (const uint64_t& addr : ret_inst_addrs) {
            spec.attach_type = BPFProbeAttachType::kEntry;
            spec.address = addr;
            PX_RETURN_IF_ERROR(LogAndAttachUProbe(spec));
            ++uprobe_count;
          }
          break;
        }
        default:
          LOG(DFATAL) << "Invalid attach type in switch statement.";
      }
    }
  }
  return uprobe_count;
}

Status UProbeManager::UpdateOpenSSLSymAddrs(obj_tools::RawFptrManager* fptr_manager,
                                            std::filesystem::path libcrypto_path, uint32_t pid) {
  PX_ASSIGN_OR_RETURN(struct openssl_symaddrs_t symaddrs,
                      OpenSSLSymAddrs(fptr_manager, libcrypto_path, pid));

  PX_RETURN_IF_ERROR(openssl_symaddrs_map_->SetValue(pid, symaddrs));

  return Status::OK();
}

Status UProbeManager::UpdateGoCommonSymAddrs(ElfReader* elf_reader, DwarfReader* dwarf_reader,
                                             const std::vector<int32_t>& pids) {
  PX_ASSIGN_OR_RETURN(struct go_common_symaddrs_t symaddrs,
                      GoCommonSymAddrs(elf_reader, dwarf_reader));

  for (auto& pid : pids) {
    PX_RETURN_IF_ERROR(go_common_symaddrs_map_->SetValue(pid, symaddrs));
  }

  return Status::OK();
}

Status UProbeManager::UpdateGoHTTP2SymAddrs(ElfReader* elf_reader, DwarfReader* dwarf_reader,
                                            const std::vector<int32_t>& pids) {
  PX_ASSIGN_OR_RETURN(struct go_http2_symaddrs_t symaddrs,
                      GoHTTP2SymAddrs(elf_reader, dwarf_reader));

  for (auto& pid : pids) {
    PX_RETURN_IF_ERROR(go_http2_symaddrs_map_->SetValue(pid, symaddrs));
  }

  return Status::OK();
}

Status UProbeManager::UpdateGoTLSSymAddrs(ElfReader* elf_reader, DwarfReader* dwarf_reader,
                                          const std::vector<int32_t>& pids) {
  PX_ASSIGN_OR_RETURN(struct go_tls_symaddrs_t symaddrs, GoTLSSymAddrs(elf_reader, dwarf_reader));

  for (auto& pid : pids) {
    PX_RETURN_IF_ERROR(go_tls_symaddrs_map_->SetValue(pid, symaddrs));
  }

  return Status::OK();
}

Status UProbeManager::UpdateNodeTLSWrapSymAddrs(int32_t pid, const std::filesystem::path& node_exe,
                                                const SemVer& ver) {
  PX_ASSIGN_OR_RETURN(struct node_tlswrap_symaddrs_t symbol_offsets,
                      NodeTLSWrapSymAddrs(node_exe, ver));
  PX_RETURN_IF_ERROR(node_tlswrap_symaddrs_map_->SetValue(pid, symbol_offsets));
  return Status::OK();
}

enum class HostPathForPIDPathSearchType { kSearchTypeEndsWith, kSearchTypeContains };

// Find the paths for some libraries, which may be inside of a container.
// Return those paths as a vector, in the same order that they came in as function arguments.
// e.g. input: lib_names = {"libssl.so.1.1", "libcrypto.so.1.1"}
// output: {"/usr/lib/mount/abc...def/usr/lib/libssl.so.1.1",
// "/usr/lib/mount/abc...def/usr/lib/libcrypto.so.1.1"}
StatusOr<std::vector<std::filesystem::path>> FindHostPathForPIDLibs(
    const std::vector<std::string_view>& lib_names, uint32_t pid, system::ProcParser* proc_parser,
    HostPathForPIDPathSearchType search_type) {
  // TODO(jps): use a mutable map<string, path> as the function argument.
  // i.e. mapping from lib_name to lib_path.
  // This would relieve the caller of the burden of tracking which entry
  // in the vector belonged to which library it wanted to find.

  PX_ASSIGN_OR_RETURN(absl::flat_hash_set<std::string> mapped_lib_paths,
                      proc_parser->GetMapPaths(pid));

  // container_libs: final function output.
  // found_vector: tracks the found status of each lib.
  // Initialize the return vector with empty paths,
  // and setup our state to "nothing found yet"
  std::vector<std::filesystem::path> container_libs(lib_names.size());
  std::vector<bool> found_vector(lib_names.size(), false);

  for (const auto& [lib_idx, lib_name] : Enumerate(lib_names)) {
    if (found_vector[lib_idx]) {
      // This lib has already been found,
      // do not search through the mapped lib paths found by GetMapPaths.
      continue;
    }

    for (const auto& mapped_lib_path : mapped_lib_paths) {
      if (HostPathForPIDPathSearchType::kSearchTypeEndsWith == search_type) {
        if (!absl::EndsWith(mapped_lib_path, lib_name)) {
          continue;
        }
      } else if (HostPathForPIDPathSearchType::kSearchTypeContains == search_type) {
        if (!absl::StrContains(mapped_lib_path, lib_name)) {
          continue;
        }
      }

      // We found a mapped_lib_path that matches to the desired lib_name.
      const auto container_lib_path = ProcPidRootPath(pid, mapped_lib_path);

      // Assign the resolved path into the output vector at the appropriate index.
      // Update found status,
      // and continue to search current set of mapped libs for next desired lib.
      container_libs[lib_idx] = container_lib_path;
      found_vector[lib_idx] = true;
      VLOG(1) << absl::Substitute("Resolved lib $0 to $1", lib_name,
                                  container_libs[lib_idx].string());
      break;
    }
  }
  return container_libs;
}

StatusOr<std::vector<std::filesystem::path>> FindHostPathForPIDLibs(
    const std::vector<std::string_view>& lib_names, uint32_t pid, system::ProcParser* proc_parser) {
  return FindHostPathForPIDLibs(lib_names, pid, proc_parser,
                                HostPathForPIDPathSearchType::kSearchTypeEndsWith);
}

enum class SSLSocketFDAccess {
  // Specifies that a connection's socket fd will be identified by accessing struct members
  // of the SSL struct exposed by OpenSSL's API when the SSL_write/SSL_read functions are called.
  kUserSpaceOffsets,
  // Specifies that a connection's socket fd will be identified based on the underlying syscall
  // (read, write, etc) while a user space tls function is on the stack.
  kNestedSyscall,
};

// SSLLibMatcher allows customizing the search of shared object files
// that need to be traced with the SSL_write and SSL_read uprobes.
// In dynamically linked cases, it's likely that there are two
// shared libraries (libssl and libcrypto). In constrast, statically
// linked cases are contained within the same binary.
struct SSLLibMatcher {
  std::string_view libssl;
  std::string_view libcrypto;
  HostPathForPIDPathSearchType search_type;
  SSLSocketFDAccess socket_fd_access;
};

constexpr char kLibSSL_1_1[] = "libssl.so.1.1";
constexpr char kLibSSL_3[] = "libssl.so.3";
constexpr char kLibPython[] = "libpython";

static constexpr const auto kLibSSLMatchers = MakeArray<SSLLibMatcher>({
    SSLLibMatcher{
        .libssl = kLibSSL_1_1,
        .libcrypto = "libcrypto.so.1.1",
        .search_type = HostPathForPIDPathSearchType::kSearchTypeEndsWith,
        .socket_fd_access = SSLSocketFDAccess::kNestedSyscall,
    },
    SSLLibMatcher{
        .libssl = kLibSSL_3,
        .libcrypto = "libcrypto.so.3",
        .search_type = HostPathForPIDPathSearchType::kSearchTypeEndsWith,
        .socket_fd_access = SSLSocketFDAccess::kNestedSyscall,
    },
    SSLLibMatcher{
        // This must match independent of python version and INSTSONAME suffix
        // (e.g. libpython3.10.so.0.1).
        .libssl = kLibPython,
        .libcrypto = kLibPython,
        .search_type = HostPathForPIDPathSearchType::kSearchTypeContains,
        .socket_fd_access = SSLSocketFDAccess::kNestedSyscall,
    },
    // non BIO native TLS applications cannot be probed by accessing the socket fd
    // within the underlying syscall.
    SSLLibMatcher{
        .libssl = kLibNettyTcnativePrefix,
        .libcrypto = kLibNettyTcnativePrefix,
        .search_type = HostPathForPIDPathSearchType::kSearchTypeContains,
        .socket_fd_access = SSLSocketFDAccess::kUserSpaceOffsets,
    },
});

ssl_source_t SSLSourceFromLib(std::string_view libssl) {
  if (libssl == kLibSSL_1_1) {
    return kLibSSL_1_1_Source;
  } else if (libssl == kLibSSL_3) {
    return kLibSSL_3_Source;
  } else if (libssl == kLibPython) {
    return kLibPythonSource;
  } else if (libssl == kLibNettyTcnativePrefix) {
    return kLibNettyTcnativeSource;
  }

  DCHECK(false) << "Unable to find matching ssl_source_t for library matcher: " << libssl;

  return kSSLUnspecified;
}

std::string ProbeFuncForSocketAccessMethod(std::string_view probe_fn,
                                           SSLSocketFDAccess socket_fd_access) {
  std::string probe_suffix = "";
  switch (socket_fd_access) {
    case SSLSocketFDAccess::kUserSpaceOffsets:
      break;
    case SSLSocketFDAccess::kNestedSyscall:
      probe_suffix = "_syscall_fd_access";
  }

  return absl::StrCat(probe_fn, probe_suffix);
}

// Return error if something unexpected occurs.
// Return 0 if nothing unexpected, but there is nothing to deploy (e.g. no OpenSSL detected).
StatusOr<int> UProbeManager::AttachOpenSSLUProbesOnDynamicLib(uint32_t pid) {
  for (auto ssl_library_match : kLibSSLMatchers) {
    const auto libssl = ssl_library_match.libssl;
    const auto libcrypto = ssl_library_match.libcrypto;

    const std::vector<std::string_view> lib_names = {libssl, libcrypto};
    const auto search_type = ssl_library_match.search_type;

    // Find paths to libssl.so and libcrypto.so for the pid, if they are in use (i.e. mapped).
    PX_ASSIGN_OR_RETURN(const std::vector<std::filesystem::path> container_lib_paths,
                        FindHostPathForPIDLibs(lib_names, pid, proc_parser_.get(), search_type));

    std::filesystem::path container_libssl = container_lib_paths[0];
    std::filesystem::path container_libcrypto = container_lib_paths[1];

    if ((container_libssl.empty() || container_libcrypto.empty())) {
      // Looks like this process doesn't have dynamic OpenSSL library installed, because it did not
      // map both of libssl.so.x.x & libcrypto.so.x.x or another compatible library.
      // Move on to the next possible SSL library. This is not an error.
      continue;
    }

    // Convert to host path, in case we're running inside a container ourselves.
    container_libssl = ProcPidRootPath(pid, container_libssl);
    container_libcrypto = ProcPidRootPath(pid, container_libcrypto);

    if (!fs::Exists(container_libssl)) {
      return error::Internal("libssl not found [path = $0]", container_libssl.string());
    }
    if (!fs::Exists(container_libcrypto)) {
      return error::Internal("libcrypto not found [path = $0]", container_libcrypto.string());
    }

    if (libssl == kLibNettyTcnativePrefix) {
      auto fptr_manager = std::make_unique<obj_tools::RawFptrManager>(container_libcrypto);

      PX_RETURN_IF_ERROR(UpdateOpenSSLSymAddrs(fptr_manager.get(), container_libcrypto, pid));
    }

    // Only try probing .so files that we haven't already set probes on.
    auto result = openssl_probed_binaries_.insert(container_libssl);
    if (!result.second) {
      return 0;
    }

    auto ssl_source = SSLSourceFromLib(libssl);
    // Optimisitcally update the SSL lib source since the probes can trigger
    // before the BPF map is updated. This value is cleaned up when the upid is
    // terminated, so if attachment fails it will be deleted prior to the pid being
    // reused.
    PX_RETURN_IF_ERROR(openssl_source_map_->SetValue(pid, ssl_source));
    for (auto spec : kOpenSSLUProbes) {
      spec.binary_path = container_libssl.string();
      spec.probe_fn =
          ProbeFuncForSocketAccessMethod(spec.probe_fn, ssl_library_match.socket_fd_access);

      PX_RETURN_IF_ERROR(LogAndAttachUProbe(spec));
    }
  }
  return kOpenSSLUProbes.size();
}

namespace {

StatusOr<SemVer> GetNodeVersion(pid_t node_pid, const std::filesystem::path& node_exe) {
  SubProcess node_version_proc(node_pid);
  PX_RETURN_IF_ERROR(node_version_proc.Start({node_exe.string(), "--version"}));
  // Wont check the exit code since we are only interested in the output.
  node_version_proc.Wait(/*close_pipe*/ false);

  std::string ver_str;
  // Wait subprocess to finish and then get stdout, to avoid race condition.
  PX_RETURN_IF_ERROR(node_version_proc.Stdout(&ver_str));
  PX_ASSIGN_OR_RETURN(SemVer ver, GetSemVer(ver_str));
  return ver;
}

}  // namespace

StatusOr<std::array<UProbeTmpl, 6>> UProbeManager::GetNodeOpensslUProbeTmpls(const SemVer& ver) {
  static const std::map<SemVer, std::array<UProbeTmpl, 6>> kNodeVersionUProbeTmpls = {
      {SemVer{12, 3, 1}, kNodeOpenSSLUProbeTmplsV12_3_1},
      {SemVer{15, 0, 0}, kNodeOpenSSLUProbeTmplsV15_0_0},
  };
  auto iter = Floor(kNodeVersionUProbeTmpls, ver);
  if (iter == kNodeVersionUProbeTmpls.end()) {
    return error::NotFound("The nodejs version cannot be older than 12.3.1, got '$0'",
                           ver.ToString());
  }
  return iter->second;
}

StatusOr<int> UProbeManager::AttachOpenSSLUProbesOnStaticBinary(const uint32_t pid) {
  PX_ASSIGN_OR_RETURN(const std::filesystem::path proc_exe, proc_parser_->GetExePath(pid));
  const auto host_proc_exe = ProcPidRootPath(pid, proc_exe);

  PX_ASSIGN_OR_RETURN(auto elf_reader, ElfReader::Create(host_proc_exe));
  auto statusor = elf_reader->SearchTheOnlySymbol("SSL_write");

  if (error::IsNotFound(statusor.status())) {
    return 0;
  }
  PX_RETURN_IF_ERROR(statusor);

  for (auto spec : kOpenSSLUProbes) {
    spec.binary_path = host_proc_exe.string();
    spec.probe_fn = absl::StrCat(spec.probe_fn, "_syscall_fd_access");
    PX_RETURN_IF_ERROR(LogAndAttachUProbe(spec));
  }
  return kOpenSSLUProbes.size();
}

StatusOr<int> UProbeManager::AttachNodeJsOpenSSLUprobes(const uint32_t pid) {
  PX_ASSIGN_OR_RETURN(const std::filesystem::path proc_exe, proc_parser_->GetExePath(pid));

  if (DetectApplication(proc_exe) != Application::kNode) {
    return 0;
  }

  const auto host_proc_exe = ProcPidRootPath(pid, proc_exe);

  const auto [_, inserted] = nodejs_binaries_.insert(host_proc_exe.string());
  if (!inserted) {
    // This is not a new binary, so nothing more to do.
    return 0;
  }

  PX_ASSIGN_OR_RETURN(const SemVer ver, GetNodeVersion(pid, proc_exe));
  PX_RETURN_IF_ERROR(UpdateNodeTLSWrapSymAddrs(pid, host_proc_exe, ver));

  // Optimisitcally update the SSL lib source since the probes can trigger
  // before the BPF map is updated. This value is cleaned up when the upid is
  // terminated, so if attachment fails it will be deleted prior to the pid being
  // reused.
  PX_RETURN_IF_ERROR(openssl_source_map_->SetValue(pid, kNodeJSSource));

  // These probes are attached on OpenSSL dynamic library (if present) as well.
  // Here they are attached on statically linked OpenSSL library (eg. for node).
  for (auto spec : kOpenSSLUProbes) {
    spec.binary_path = host_proc_exe.string();
    PX_RETURN_IF_ERROR(LogAndAttachUProbe(spec));
  }

  // These are node-specific probes.
  PX_ASSIGN_OR_RETURN(auto uprobe_tmpls, GetNodeOpensslUProbeTmpls(ver));
  PX_ASSIGN_OR_RETURN(auto elf_reader, ElfReader::Create(host_proc_exe));
  PX_ASSIGN_OR_RETURN(int count, AttachUProbeTmpl(uprobe_tmpls, host_proc_exe, elf_reader.get()));

  return kOpenSSLUProbes.size() + count;
}

StatusOr<int> UProbeManager::AttachGoTLSUProbes(const std::string& binary,
                                                obj_tools::ElfReader* elf_reader,
                                                obj_tools::DwarfReader* dwarf_reader,
                                                const std::vector<int32_t>& pids) {
  // Step 1: Update BPF symbols_map on all new PIDs.
  Status s = UpdateGoTLSSymAddrs(elf_reader, dwarf_reader, pids);
  if (!s.ok()) {
    // Doesn't appear to be a binary with the mandatory symbols.
    // Might not even be a golang binary.
    // Either way, not of interest to probe.
    return 0;
  }

  // Step 2: Deploy uprobes on all new binaries.
  auto result = go_tls_probed_binaries_.insert(binary);
  if (!result.second) {
    // This is not a new binary, so nothing more to do.
    return 0;
  }
  return AttachUProbeTmpl(kGoTLSUProbeTmpls, binary, elf_reader);
}

StatusOr<int> UProbeManager::AttachGoHTTP2UProbes(const std::string& binary,
                                                  obj_tools::ElfReader* elf_reader,
                                                  obj_tools::DwarfReader* dwarf_reader,
                                                  const std::vector<int32_t>& pids) {
  // Step 1: Update BPF symaddrs for this binary.
  Status s = UpdateGoHTTP2SymAddrs(elf_reader, dwarf_reader, pids);
  if (!s.ok()) {
    return 0;
  }

  // Step 2: Deploy uprobes on all new binaries.
  auto result = go_http2_probed_binaries_.insert(binary);
  if (!result.second) {
    // This is not a new binary, so nothing more to do.
    return 0;
  }
  return AttachUProbeTmpl(kHTTP2ProbeTmpls, binary, elf_reader);
}

namespace {

// Convert PID list from list of UPIDs to a map with key=binary name, value=PIDs
std::map<std::string, std::vector<int32_t>> ConvertPIDsListToMap(
    const absl::flat_hash_set<md::UPID>& upids) {
  const system::ProcParser proc_parser;

  // Convert to a map of binaries, with the upids that are instances of that binary.
  std::map<std::string, std::vector<int32_t>> pids;

  for (const auto& upid : upids) {
    // TODO(yzhao): Might need to check the start time.
    PX_ASSIGN_OR(const auto exe_path, proc_parser.GetExePath(upid.pid()), continue);
    const auto host_exe_path = ProcPidRootPath(upid.pid(), exe_path);

    if (!fs::Exists(host_exe_path)) {
      continue;
    }
    pids[host_exe_path.string()].push_back(upid.pid());
  }

  VLOG(1) << absl::Substitute("New PIDs count = $0", pids.size());

  return pids;
}

}  // namespace

std::thread UProbeManager::RunDeployUProbesThread(const absl::flat_hash_set<md::UPID>& upids) {
  // Increment before starting thread to avoid race in case thread starts late.
  // And, capture upids by *copy* in case this thread outlives the connector context
  // that passed in the const ref. of upids.
  ++num_deploy_uprobes_threads_;
  return std::thread([this, upids]() {
    DeployUProbes(upids);
    --num_deploy_uprobes_threads_;
  });
  return {};
}

void UProbeManager::CleanupPIDMaps(const absl::flat_hash_set<md::UPID>& deleted_upids) {
  for (const auto& pid : deleted_upids) {
    PX_UNUSED(openssl_source_map_->RemoveValue(pid.pid()));
    PX_UNUSED(openssl_symaddrs_map_->RemoveValue(pid.pid()));
    PX_UNUSED(go_common_symaddrs_map_->RemoveValue(pid.pid()));
    PX_UNUSED(go_tls_symaddrs_map_->RemoveValue(pid.pid()));
    PX_UNUSED(go_http2_symaddrs_map_->RemoveValue(pid.pid()));
    PX_UNUSED(node_tlswrap_symaddrs_map_->RemoveValue(pid.pid()));
  }
}

int UProbeManager::DeployOpenSSLUProbes(const absl::flat_hash_set<md::UPID>& pids) {
  int uprobe_count = 0;

  // TODO(yzhao): Change to use ConvertPIDsListToMap() to avoid processing the same executable
  // multiple times for different processes.
  for (const auto& pid : pids) {
    if (cfg_disable_self_probing_ && pid.pid() == static_cast<uint32_t>(getpid())) {
      continue;
    }

    auto count_or = AttachOpenSSLUProbesOnDynamicLib(pid.pid());
    if (count_or.ok()) {
      uprobe_count += count_or.ValueOrDie();
      VLOG(1) << absl::Substitute(
          "Attaching OpenSSL uprobes on dynamic library succeeded for PID $0: $1 probes", pid.pid(),
          count_or.ValueOrDie());
    } else {
      monitor_.AppendSourceStatusRecord("socket_tracer", count_or.status(),
                                        "AttachOpenSSLUprobesOnDynamicLib");
      VLOG(1) << absl::Substitute(
          "Attaching OpenSSL uprobes on dynamic library failed for PID $0: $1", pid.pid(),
          count_or.ToString());
    }

    count_or = AttachNodeJsOpenSSLUprobes(pid.pid());
    if (count_or.ok()) {
      uprobe_count += count_or.ValueOrDie();
      VLOG(1) << absl::Substitute(
          "Attaching OpenSSL uprobes on NodeJS (statically linked OpenSSL) succeeded for "
          "PID $0: $1 probes",
          pid.pid(), count_or.ValueOrDie());
    } else {
      monitor_.AppendSourceStatusRecord("socket_tracer", count_or.status(),
                                        "AttachNodeJsOpenSSLUprobes");
      VLOG(1) << absl::Substitute(
          "Attaching OpenSSL uprobes on NodeJS (statically linked OpenSSL) failed for "
          "PID $0: $1",
          pid.pid(), count_or.ToString());
    }

    // Attach uprobes to statically linked applications only if no other probes have been attached.
    if (FLAGS_stirling_trace_static_tls_binaries && count_or.ok() && count_or.ValueOrDie() == 0) {
      // Optimisitcally update the SSL lib source since the probes can trigger
      // before the BPF map is updated. This value is cleaned up when the upid is
      // terminated, so if attachment fails it will be deleted prior to the pid being
      // reused.
      PX_UNUSED(openssl_source_map_->SetValue(pid.pid(), kStaticallyLinkedSource));
      count_or = AttachOpenSSLUProbesOnStaticBinary(pid.pid());

      if (count_or.ok()) {
        uprobe_count += count_or.ValueOrDie();

        VLOG(1) << absl::Substitute(
            "Attaching OpenSSL uprobes on executable statically linked OpenSSL library"
            "succeeded for PID $0: $1 probes",
            pid.pid(), count_or.ValueOrDie());
      } else {
        monitor_.AppendSourceStatusRecord("socket_tracer", count_or.status(),
                                          "AttachOpenSSLUprobesStaticBinary");
        VLOG(1) << absl::Substitute(
            "Attaching OpenSSL uprobes on executable statically linked OpenSSL library failed"
            "for PID $0: $1",
            pid.pid(), count_or.ToString());
      }
    }
  }

  return uprobe_count;
}

StatusOr<std::string> UProbeManager::MD5onFile(const std::string& file) {
  // Implementation based on
  // https://stackoverflow.com/questions/1220046/how-to-get-the-md5-hash-of-a-file-in-c
  unsigned char md5_hash[MD5_DIGEST_LENGTH] = {0};
  int file_descript = open(file.c_str(), O_RDONLY);
  if (-1 == file_descript) {
    return error::Internal(absl::Substitute(
        "Failed to get the MD5 hash of file $0 because of open failure. errno $1.", file, errno));
  }

  struct stat statbuf;
  if (-1 == fstat(file_descript, &statbuf)) {
    close(file_descript);  // Ignore if close fails, we already exit the function with an error on
                           // the file.
    return error::Internal(absl::Substitute(
        "Failed to get the MD5 hash of file $0 because of stat failure. errno $1.", file, errno));
  }
  uint64_t file_size = statbuf.st_size;

  void* mapped_file_buffer =
      mmap(/*addr*/ 0, file_size, PROT_READ, MAP_SHARED, file_descript, /*offset*/ 0);
  if (MAP_FAILED == mapped_file_buffer) {
    return error::Internal(absl::Substitute(
        "Failed to map area to store file $0 that needs hashing. errno $1.", file, errno));
  }
  if (-1 == close(file_descript)) {
    return error::Internal(
        absl::Substitute("Failed to close file $0 that needs hashing. errno $1.", file, errno));
  }
  // This can't fail, it always returns the pointer to the hash value (3rd argument).
  MD5((unsigned char*)mapped_file_buffer, file_size, md5_hash);
  if (0 != munmap(mapped_file_buffer, file_size)) {
    return error::Internal(
        absl::Substitute("Failed to unmap file $0 that needs hashing. errno $1.", file, errno));
  }

  std::basic_string_view<char> md5_hash_str_view{reinterpret_cast<char*>(md5_hash),
                                                 MD5_DIGEST_LENGTH};
  std::string hash_str =
      absl::AsciiStrToLower(BytesToString<bytes_format::HexCompact>(md5_hash_str_view));

  return hash_str;
}

StatusOr<int> UProbeManager::AttachGrpcCUProbesOnDynamicPythonLib(uint32_t pid) {
  // grpc-c libraries that are used by python normally have this prefix,
  // I have not seen a case where it's not used.
  static constexpr std::string_view kGrpcCPythonLibPrefix = "cygrpc.cpython";
  const std::vector<std::string_view> lib_names = {kGrpcCPythonLibPrefix};

  // Find path to grpc-c shared object, if it's used (i.e. mapped).
  PX_ASSIGN_OR_RETURN(const std::vector<std::filesystem::path> container_lib_paths,
                      FindHostPathForPIDLibs(lib_names, pid, proc_parser_.get(),
                                             HostPathForPIDPathSearchType::kSearchTypeContains));

  std::filesystem::path container_libgrpcc = container_lib_paths[0];

  if (container_libgrpcc.empty()) {
    // Looks like this process doesn't have dynamic grpc-c library mapped.
    return 0;
  }

  // Convert to host path, in case we're running inside a container ourselves.
  container_libgrpcc = ProcPidRootPath(pid, container_libgrpcc);
  if (!fs::Exists(container_libgrpcc)) {
    return error::Internal("grpc-c library not found [path=$0 pid=$1]", container_libgrpcc.string(),
                           pid);
  }

  // Only try probing .so files that we haven't already set probes on.
  auto result = grpc_c_probed_binaries_.insert(container_libgrpcc);
  if (!result.second) {
    return 0;
  }

  // Calculate MD5 hash of the grpc-c library to know which version it is.
  // For further explanation see the definition of kGrpcCMD5HashToVersion.
  PX_ASSIGN_OR_RETURN(const std::string hash_str, MD5onFile(container_libgrpcc.string()));
  VLOG(1) << absl::Substitute("Found MD5 hash $0 of library $1 for pid=$2", hash_str,
                              container_libgrpcc.string(), pid);

  // Find the version of the library by its MD5 hash.
  auto iter = kGrpcCMD5HashToVersion.find(hash_str);
  if (iter == kGrpcCMD5HashToVersion.end()) {
    return error::Unimplemented("Unknown MD5 hash $0 of library $1 and pid=$2.", hash_str,
                                container_libgrpcc.string(), pid);
  }
  const enum grpc_c_version_t version = iter->second;
  VLOG(1) << absl::Substitute("Updating gRPC-C version of pid $0 to $1", pid,
                              magic_enum::enum_name(version));

  PX_RETURN_IF_ERROR(grpc_c_versions_map_->SetValue(pid, version));

  // Attach the needed probes.
  // This currently works only for non-stripped versions of the shared object.
  VLOG(1) << absl::Substitute("Attaching GRPC-C uprobes to $0 for pid $1",
                              container_libgrpcc.string(), pid);
  for (auto spec : kGrpcCUProbes) {
    spec.binary_path = container_libgrpcc.string();
    auto return_value = bcc_->AttachUProbe(spec);
    if (!return_value.ok()) {
      LOG(WARNING) << absl::Substitute("Failed to attach gRPC-C probe $0 to pid $1 and file $2",
                                       spec.symbol, pid, container_libgrpcc.string());
      return return_value;
    }
  }
  bool attached_data_parser_parse_probe = false;
  for (auto spec : kGrpcCDataParserParseUProbes) {
    spec.binary_path = container_libgrpcc.string();
    auto return_value = bcc_->AttachUProbe(spec);
    if (return_value.ok()) {
      attached_data_parser_parse_probe = true;
      break;
    }
  }
  if (!attached_data_parser_parse_probe) {
    return error::Internal("Failed to attach a data parser parse probe, pid=$0.", pid);
  }

  VLOG(1) << absl::Substitute("Successfully attached $0 gRPC-C probes to pid $1",
                              kGrpcCUProbes.size(), pid);

  return kGrpcCUProbes.size() + 1;  // +1 for the data parser parse probe.
}

int UProbeManager::DeployGrpcCUProbes(const absl::flat_hash_set<md::UPID>& pids) {
  int uprobe_count = 0;
  for (const auto& pid : pids) {
    if (cfg_disable_self_probing_ && pid.pid() == static_cast<uint32_t>(getpid())) {
      continue;
    }

    auto count_or = AttachGrpcCUProbesOnDynamicPythonLib(pid.pid());
    if (!count_or.ok()) {
      VLOG(1) << absl::Substitute(
          "Attaching gRPC-C uprobes on dynamic python library failed for PID $0: $1", pid.pid(),
          count_or.ToString());
      continue;
    }

    uprobe_count += count_or.ValueOrDie();
    VLOG(1) << absl::Substitute(
        "Attaching gRPC-C uprobes on dynamic python library succeeded for PID $0: $1 probes",
        pid.pid(), count_or.ValueOrDie());
  }

  return uprobe_count;
}

int UProbeManager::DeployGoUProbes(const absl::flat_hash_set<md::UPID>& pids) {
  int uprobe_count = 0;

  static int32_t kPID = getpid();

  for (const auto& [binary, pid_vec] : ConvertPIDsListToMap(pids)) {
    // Don't bother rescanning binaries that have been scanned before to avoid unnecessary work.
    if (!scanned_binaries_.insert(binary).second) {
      continue;
    }

    if (cfg_disable_self_probing_) {
      // Don't try to attach uprobes to self.
      // This speeds up stirling_wrapper initialization significantly.
      if (pid_vec.size() == 1 && pid_vec[0] == kPID) {
        continue;
      }
    }

    // Read binary's symbols.
    StatusOr<std::unique_ptr<ElfReader>> elf_reader_status = ElfReader::Create(binary);
    if (!elf_reader_status.ok()) {
      LOG(WARNING) << absl::Substitute(
          "Cannot analyze binary $0 for uprobe deployment. "
          "If file is under /var/lib, container may have terminated. "
          "Message = $1",
          binary, elf_reader_status.msg());
      continue;
    }
    std::unique_ptr<ElfReader> elf_reader = elf_reader_status.ConsumeValueOrDie();

    // Avoid going past this point if not a golang program.
    // The DwarfReader is memory intensive, and the remaining probes are Golang specific.
    if (!IsGoExecutable(elf_reader.get())) {
      continue;
    }

    StatusOr<std::unique_ptr<DwarfReader>> dwarf_reader_status =
        DwarfReader::CreateIndexingAll(binary);
    if (!dwarf_reader_status.ok()) {
      VLOG(1) << absl::Substitute(
          "Failed to get binary $0 debug symbols. Cannot deploy uprobes. "
          "Message = $1",
          binary, dwarf_reader_status.msg());
      continue;
    }
    std::unique_ptr<DwarfReader> dwarf_reader = dwarf_reader_status.ConsumeValueOrDie();
    Status s = UpdateGoCommonSymAddrs(elf_reader.get(), dwarf_reader.get(), pid_vec);
    if (!s.ok()) {
      VLOG(1) << absl::Substitute(
          "Golang binary $0 does not have the mandatory symbols (e.g. TCPConn).", binary);
      continue;
    }

    // GoTLS Probes.
    if (!cfg_disable_go_tls_tracing_) {
      StatusOr<int> attach_status =
          AttachGoTLSUProbes(binary, elf_reader.get(), dwarf_reader.get(), pid_vec);
      if (!attach_status.ok()) {
        monitor_.AppendSourceStatusRecord("socket_tracer", attach_status.status(),
                                          "AttachGoTLSUProbes");
        LOG_FIRST_N(WARNING, 10) << absl::Substitute("Failed to attach GoTLS Uprobes to $0: $1",
                                                     binary, attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }

    // Go HTTP2 Probes.
    if (!cfg_disable_go_tls_tracing_ && cfg_enable_http2_tracing_) {
      StatusOr<int> attach_status =
          AttachGoHTTP2UProbes(binary, elf_reader.get(), dwarf_reader.get(), pid_vec);
      if (!attach_status.ok()) {
        monitor_.AppendSourceStatusRecord("socket_tracer", attach_status.status(),
                                          "AttachGoHTTP2UProbes");
        LOG_FIRST_N(WARNING, 10) << absl::Substitute("Failed to attach HTTP2 Uprobes to $0: $1",
                                                     binary, attach_status.ToString());
      } else {
        uprobe_count += attach_status.ValueOrDie();
      }
    }
  }

  return uprobe_count;
}

absl::flat_hash_set<md::UPID> UProbeManager::PIDsToRescanForUProbes() {
  // Count number of calls to this function.
  ++rescan_counter_;

  // Get the ASID, using an entry from proc_tracker.
  if (proc_tracker_.upids().empty()) {
    return {};
  }
  uint32_t asid = proc_tracker_.upids().begin()->asid();

  absl::flat_hash_set<md::UPID> upids_to_rescan;
  for (const auto& pid : upids_with_mmap_) {
    md::UPID upid(asid, pid.pid, pid.start_time_ticks);

    if (proc_tracker_.upids().contains(upid) && !proc_tracker_.new_upids().contains(upid)) {
      // Filter out upids_to_rescan based on a backoff that is tracked per UPID.
      // Each UPID has a modulus, which defines the periodicity at which it can rescan.
      // This periodicity is used in a modulo operation, hence the term modulus.
      constexpr int kInitialModulus = 1;
      constexpr int kMaximumModulus = 1 << 12;
      const double kBackoffFactor = FLAGS_stirling_rescan_exp_backoff_factor;

      auto [iter, success] = backoff_map_.emplace(upid, kInitialModulus);
      int& modulus = iter->second;
      DCHECK_NE(modulus, 0) << success;

      // Each PID has a backoff period that exponentially grows since the last attempted rescan.
      // The simple version would be:
      //   if (rescan_counter_ % modulus  == 0)
      // But this could cause a bunch of pids to be added to the rescan list in the same iteration.
      // Jitter this by comparing to the modulus to the pid:
      //   if ((rescan_counter_ % modulus) == (upid.pid() % modulus))
      if ((rescan_counter_ % modulus) == static_cast<int>(upid.pid() % modulus)) {
        upids_to_rescan.insert(upid);

        // Increase backoff period according to an exponential back-off.
        modulus = std::min(static_cast<int>(modulus * kBackoffFactor), kMaximumModulus);
      }
    }
  }

  upids_with_mmap_.clear();

  return upids_to_rescan;
}

bool KernelVersionAllowsGRPCCTracing() {
  constexpr KernelVersion kKernelVersion5_3 = {5, 3, 0};
  auto kernel_version_or = GetKernelVersion();
  if (kernel_version_or.ok()) {
    auto kernel_version = kernel_version_or.ValueOrDie();
    auto order = CompareKernelVersions(kernel_version, kKernelVersion5_3);
    return order == KernelVersionOrder::kSame || order == KernelVersionOrder::kNewer;
  }
  return false;
}

void UProbeManager::DeployUProbes(const absl::flat_hash_set<md::UPID>& pids) {
  const std::lock_guard<std::mutex> lock(deploy_uprobes_mutex_);

  proc_tracker_.Update(pids);

  // Before deploying new probes, clean-up map entries for old processes that are now dead.
  CleanupPIDMaps(proc_tracker_.deleted_upids());

  int uprobe_count = 0;

  uprobe_count += DeployOpenSSLUProbes(proc_tracker_.new_upids());

  if (FLAGS_stirling_enable_grpc_c_tracing && KernelVersionAllowsGRPCCTracing()) {
    uprobe_count += DeployGrpcCUProbes(proc_tracker_.new_upids());
  }

  if (FLAGS_stirling_rescan_for_dlopen) {
    auto pids_to_rescan_for_uprobes = PIDsToRescanForUProbes();
    uprobe_count += DeployOpenSSLUProbes(pids_to_rescan_for_uprobes);
    if (FLAGS_stirling_enable_grpc_c_tracing && KernelVersionAllowsGRPCCTracing()) {
      uprobe_count += DeployGrpcCUProbes(pids_to_rescan_for_uprobes);
    }
  }

  uprobe_count += DeployGoUProbes(proc_tracker_.new_upids());

  if (uprobe_count != 0) {
    LOG(INFO) << absl::Substitute("Number of uprobes deployed = $0", uprobe_count);
  }
}

}  // namespace stirling
}  // namespace px
