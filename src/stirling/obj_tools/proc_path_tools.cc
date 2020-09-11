#include "src/stirling/obj_tools/proc_path_tools.h"

#include <filesystem>
#include <memory>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"
#include "src/stirling/obj_tools/elf_tools.h"

namespace pl {
namespace stirling {
namespace obj_tools {

// Example #1: regular process not in container
//   ResolveProcessRootDir():   <empty>
//
// Example #2: container with an overlay on / (as discovered through /proc/pid/mounts)
//   ResolveProcessRootDir():   /var/lib/docker/overlay2/402fe2...be0/merged
pl::StatusOr<std::filesystem::path> ResolveProcessRootDir(const std::filesystem::path& proc_pid) {
  pid_t pid = 0;
  if (!absl::SimpleAtoi(proc_pid.filename().string(), &pid)) {
    return error::InvalidArgument(
        "Input is not a /proc/<pid> path, because <pid> is not a number, got: $0",
        proc_pid.string());
  }
  system::ProcParser proc_parser(system::Config::GetInstance());
  return proc_parser.ResolveMountPoint(pid, "/");
}

// Example #1: regular process not in container
//   ResolveProcessPath("/proc/123", "/usr/bin/server") -> /usr/bin/server
//
// Example #2: container with an overlay on /
//   ResolveProcessPath("/proc/123", "/app/server") ->
//   /var/lib/docker/overlay2/402fe2...be0/merged/app/server
pl::StatusOr<std::filesystem::path> ResolveProcessPath(const std::filesystem::path& proc_pid,
                                                       const std::filesystem::path& path) {
  PL_ASSIGN_OR_RETURN(std::filesystem::path process_root_dir, ResolveProcessRootDir(proc_pid));

  // Warning: must use JoinPath, because we are dealing with two absolute paths.
  std::filesystem::path host_path = fs::JoinPath({&process_root_dir, &path});

  return host_path;
}

pl::StatusOr<std::filesystem::path> ResolveProcExe(const std::filesystem::path& proc_pid) {
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

pl::StatusOr<std::filesystem::path> GetPIDBinaryOnHost(uint32_t pid,
                                                       std::optional<int64_t> start_time) {
  const std::filesystem::path& host_path = system::Config::GetInstance().host_path();
  const std::filesystem::path& proc_path = system::Config::GetInstance().proc_path();

  std::filesystem::path pid_path = proc_path / std::to_string(pid);

  if (start_time.has_value()) {
    StatusOr<int64_t> pid_start_time = system::GetPIDStartTimeTicks(pid_path);
    if (!pid_start_time.ok()) {
      return error::Internal("Could not determine start time of PID $0: '$1'", pid,
                             pid_start_time.status().ToString());
    }
    if (start_time.value() != pid_start_time.ValueOrDie()) {
      return error::NotFound(
          "This is not the pid you are looking for... "
          "Start time does not match (specification: $0 vs system: $1).",
          start_time.value(), pid_start_time.ValueOrDie());
    }
  }

  PL_ASSIGN_OR_RETURN(std::filesystem::path pid_exe_path, ResolveProcExe(pid_path));

  // If we're running in a container, convert exe to be relative to our host mount.
  // Note that we mount host '/' to '/host' inside container.
  // Warning: must use JoinPath, because we are dealing with two absolute paths.
  std::filesystem::path path_on_host = fs::JoinPath({&host_path, &pid_exe_path});
  PL_RETURN_IF_ERROR(fs::Exists(path_on_host));
  return path_on_host;
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
