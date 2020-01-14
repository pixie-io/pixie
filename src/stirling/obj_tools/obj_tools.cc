#include "src/stirling/obj_tools/obj_tools.h"

#include <filesystem>
#include <memory>

#include "src/common/system/config.h"
#include "src/stirling/obj_tools/elf_tools.h"
#include "src/stirling/utils/fs_wrapper.h"

namespace pl {
namespace stirling {
namespace obj_tools {

namespace fs = std::experimental::filesystem;

namespace {

// NOTE: Today this function is not robust and looks for a very specific pattern.
// IT IS NOT PRODUCTION READY.
//
// Reads the content of the mounts file and figures out the merged path of the underlying overlay
// filesystem.
pl::StatusOr<fs::path> ResolveMergedPath(fs::path proc_pid) {
  fs::path mounts = proc_pid / "mounts";
  PL_ASSIGN_OR_RETURN(std::string mounts_content, pl::ReadFileToString(mounts));

  // The format of /proc/<pid>/mounts is described in the man page of 'fstab':
  // http://man7.org/linux/man-pages/man5/fstab.5.html

  // Each filesystem is described on a separate line.
  std::vector<std::string_view> lines =
      absl::StrSplit(mounts_content, '\n', absl::SkipWhitespace());
  if (lines.empty()) {
    return error::InvalidArgument("Mounts file '$0' is empty", mounts.string());
  }
  // Won't be empty as absl::SkipWhitespace() skips them.
  const auto& line = lines[0];

  // Fields on each line are separated by tabs or spaces.
  std::vector<std::string_view> fields = absl::StrSplit(line, absl::ByAnyChar("\t "));
  if (fields.size() < 4) {
    return error::Internal(absl::Substitute(
        "Expect at least 4 fields (separated by tabs or spaces in the content of $0, got $1",
        mounts.string(), line));
  }
  std::string_view mount_point = fields[1];
  std::string_view type = fields[2];
  std::string_view mount_options = fields[3];

  if (mount_point != "/" || type != "overlay") {
    return error::InvalidArgument("Proc PID '$0' does not have an overlay FS", proc_pid.string());
  }
  constexpr std::string_view kUpperDir = "upperdir=";
  constexpr std::string_view kDiffSuffix = "/diff";
  constexpr std::string_view kMerged = "merged";
  std::vector<std::string_view> options = absl::StrSplit(mount_options, ',');
  for (const auto& option : options) {
    auto pos = option.find(kUpperDir);
    if (pos == std::string_view::npos) {
      continue;
    }
    std::string_view s = option.substr(pos + kUpperDir.size());
    DCHECK(absl::EndsWith(option, kDiffSuffix));
    s.remove_suffix(kDiffSuffix.size());
    return fs::path(s) / kMerged;
  }
  return error::Internal("Failed to resolve merged path for $0", proc_pid.string());
}

}  // namespace

/**
 * ResolveExe takes a /proc/<pid> directory and a potentially empty host path, and resolve the
 * binary for that process.  It accounts for any overlay filesystems to resolve the exe to its
 * actual location.  This is important for exe files in containers, where the file is actually
 * located on the host at some other location.
 *
 * NOTE: Today this function is not robust and looks for a very specific pattern.
 * IT IS NOT PRODUCTION READY.
 *
 * @param proc_pid Path to process info /proc/<pid>.
 * @return The resolved path. Either the original exe symlink if no overlay fs was found, or the
 * path to the host location if an overlay was found.
 */
pl::StatusOr<fs::path> ResolveExe(fs::path proc_pid, fs::path host) {
  PL_ASSIGN_OR_RETURN(fs::path exe, utils::ReadSymlink(proc_pid/"exe"));

  PL_ASSIGN_OR_RETURN(fs::path merged, ResolveMergedPath(proc_pid));

  // If we're running in a container, convert exe to be relative to host. As we mount host's '/'
  // to '/host' inside container.
  exe = host / merged / exe;
  if (!fs::exists(exe)) {
    return error::InvalidArgument("Ignoring $0: Does not exist.", exe.string());
  }

  std::error_code ec;
  fs::path canonical_exe = fs::canonical(exe, ec);
  if (ec) {
    return error::InvalidArgument("Ignoring $0: Could not find canonical path.", exe.string());
  }
  return exe;
}

std::map<std::string, std::vector<int>> GetActiveBinaries(fs::path proc, fs::path host) {
  std::map<std::string, std::vector<int>> binaries;
  proc = host / proc;
  for (const auto& p : fs::directory_iterator(proc)) {
    VLOG(1) << absl::Substitute("Directory: $0", p.path().string());
    int pid = 0;
    if (!absl::SimpleAtoi(p.path().filename().string(), &pid)) {
      VLOG(1) << absl::Substitute("Ignoring $0: Failed to parse pid.", p.path().string());
      continue;
    }

    pl::StatusOr<fs::path> exe_or = ResolveExe(p, host);
    if (!exe_or.ok()) {
      VLOG(1) << absl::Substitute("Ignoring $0: Failed to resolve exe path, error message: $1",
                                  p.path().string(), exe_or.msg());
      continue;
    }
    fs::path exe = exe_or.ConsumeValueOrDie();
    binaries[exe].push_back(pid);
  }

  LOG(INFO) << "Number of unique binaries found: " << binaries.size();

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
