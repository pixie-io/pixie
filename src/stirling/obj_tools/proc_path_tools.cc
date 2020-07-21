#include "src/stirling/obj_tools/proc_path_tools.h"

#include <filesystem>
#include <memory>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/stirling/obj_tools/elf_tools.h"

namespace pl {
namespace stirling {
namespace obj_tools {

// Example #1: regular process not in container
//   ResolveProcessRootDir():   <empty>
//
// Example #2: container with an overlay on / (as discovered through /proc/pid/mounts)
//   ResolveProcessRootDir():   /var/lib/docker/overlay2/402fe2...be0/merged
pl::StatusOr<std::filesystem::path> ResolveProcessRootDir(std::filesystem::path proc_pid) {
  std::filesystem::path mounts = proc_pid / "mounts";
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
  // TODO(oazizi): Assumes the overlayfs is the first entry in /proc/<pid>/mounts.
  const auto& line = lines[0];

  // Fields on each line are separated by tabs or spaces.
  std::vector<std::string_view> fields = absl::StrSplit(line, absl::ByAnyChar("\t "));
  if (fields.size() < 4) {
    return error::Internal(absl::Substitute(
        "Expected at least 4 fields (separated by tabs or spaces in the content of $0, got $1",
        mounts.string(), line));
  }
  std::string_view mount_point = fields[1];
  std::string_view type = fields[2];
  std::string_view mount_options = fields[3];

  // TODO(oazizi): Today this function only works on overlayfs. Support other filesystems.
  if (mount_point != "/" || type != "overlay") {
    return std::filesystem::path{};
  }
  return GetOverlayMergedDir(mount_options);
}

pl::StatusOr<std::filesystem::path> GetOverlayMergedDir(std::string_view mount_options) {
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
    return std::filesystem::path(s) / kMerged;
  }
  return error::Internal("Failed to resolve overlay path");
}

// Example #1: regular process not in container
//   ResolveProcessPath("/proc/123", "/usr/bin/server") -> /usr/bin/server
//
// Example #2: container with an overlay on /
//   ResolveProcessPath("/proc/123", "/app/server") ->
//   /var/lib/docker/overlay2/402fe2...be0/merged/app/server
pl::StatusOr<std::filesystem::path> ResolveProcessPath(std::filesystem::path proc_pid,
                                                       std::filesystem::path path) {
  PL_ASSIGN_OR_RETURN(std::filesystem::path process_root_dir, ResolveProcessRootDir(proc_pid));

  // Warning: must use JoinPath, because we are dealing with two absolute paths.
  std::filesystem::path host_path = fs::JoinPath({&process_root_dir, &path});

  return host_path;
}

pl::StatusOr<std::filesystem::path> ResolveProcExe(std::filesystem::path proc_pid) {
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, fs::ReadSymlink(proc_pid / "exe"));
  if (proc_exe.empty() || proc_exe == "/") {
    // Not sure what causes this, but sometimes get symlinks that point to "/".
    // Seems to happen with PIDs that are short-lived, because I can never catch it in the act.
    // I suspect there is a race with the proc filesystem, with PID creation/destruction,
    // but this is not confirmed. Would be nice to understand the root cause, but for now, just
    // filter these out.
    return error::Internal("Symlink appears malformed.");
  }
  return ResolveProcessPath(proc_pid, proc_exe);
}

pl::StatusOr<std::filesystem::path> ResolveProcExe(pid_t pid) {
  return ResolveProcExe(system::Config::GetInstance().proc_path() / std::to_string(pid));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
