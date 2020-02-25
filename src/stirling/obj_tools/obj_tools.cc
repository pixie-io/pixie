#include "src/stirling/obj_tools/obj_tools.h"

#include <filesystem>
#include <memory>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/obj_tools/proc_path_tools.h"

namespace pl {
namespace stirling {
namespace obj_tools {

pl::StatusOr<std::filesystem::path> GetActiveBinary(std::filesystem::path host_path,
                                                    std::filesystem::path proc_pid) {
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, ResolveProcExe(proc_pid));

  // If we're running in a container, convert exe to be relative to our host mount.
  // Note that we mount host '/' to '/host' inside container.
  // Warning: must use JoinPath, because we are dealing with two absolute paths.
  std::filesystem::path host_exe = fs::JoinPath({&host_path, &proc_exe});
  PL_RETURN_IF_ERROR(fs::Exists(host_exe));
  return host_exe;
}

std::map<std::string, std::vector<int>> GetActiveBinaries(
    const std::filesystem::path& host_path,
    const std::map<int32_t, std::filesystem::path>& pid_paths) {
  std::map<std::string, std::vector<int>> binaries;
  for (const auto& [pid, p] : pid_paths) {
    VLOG(1) << absl::Substitute("Directory: $0", p.string());

    pl::StatusOr<std::filesystem::path> host_exe_or = GetActiveBinary(host_path, p);
    if (!host_exe_or.ok()) {
      VLOG(1) << absl::Substitute("Ignoring $0: Failed to resolve exe path, error message: $1",
                                  p.string(), host_exe_or.msg());
      continue;
    }

    binaries[host_exe_or.ValueOrDie()].push_back(pid);
  }

  LOG(INFO) << "Number of unique binaries found: " << binaries.size();
  for (const auto& b : binaries) {
    VLOG(1) << "  " << b.first;
  }

  return binaries;
}

namespace {

pl::Status PopulateSymAddrs(const std::string& binary, struct conn_symaddrs_t* symaddrs) {
  using pl::stirling::elf_tools::ElfReader;

  PL_ASSIGN_OR_RETURN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(binary));
  symaddrs->syscall_conn =
      elf_reader
          ->SymbolAddress(
              "go.itab.*google.golang.org/grpc/credentials/internal.syscallConn,net.Conn")
          .value_or(-1);
  symaddrs->tls_conn = elf_reader->SymbolAddress("go.itab.*crypto/tls.Conn,net.Conn").value_or(-1);
  symaddrs->tcp_conn = elf_reader->SymbolAddress("go.itab.*net.TCPConn,net.Conn").value_or(-1);

  return pl::Status::OK();
}

}  // namespace

std::map<uint32_t, struct conn_symaddrs_t> GetSymAddrs(
    const std::map<std::string, std::vector<int>>& binaries) {
  std::map<uint32_t, struct conn_symaddrs_t> res;
  for (auto& [binary, pid_vec] : binaries) {
    struct conn_symaddrs_t symaddrs = {};
    pl::Status s = PopulateSymAddrs(binary, &symaddrs);
    // TCP conn is mandatory by the probe, so bail if it is not found (-1).
    // It should be the last layer of nested interface, and contains the FD.
    // The other conns can be invalid, and will simply be ignored.
    if (!s.ok() || symaddrs.tcp_conn == -1) {
      continue;
    }
    for (auto& pid : pid_vec) {
      res[pid] = symaddrs;
    }
  }
  return res;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
