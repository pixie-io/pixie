#include "src/stirling/utils/proc_path_tools.h"

#include <filesystem>
#include <memory>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"

using ::pl::system::ProcParser;

namespace pl {
namespace stirling {

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
  PL_ASSIGN_OR_RETURN(std::unique_ptr<FileSystemResolver> resolver,
                      FileSystemResolver::Create(pid));
  return resolver->ResolveMountPoint("/");
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

pl::StatusOr<std::filesystem::path> ResolvePIDBinary(uint32_t pid,
                                                     std::optional<int64_t> start_time) {
  const system::Config& sysconfig = system::Config::GetInstance();

  std::filesystem::path pid_path = sysconfig.proc_path() / std::to_string(pid);

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

  return ResolveProcExe(pid_path);
}

namespace {

/**
 * Returns a path to an existent file (or directory) that is mounted to the input mount point
 * of the process specified by the pid. The process could belong to a different mount namespace
 * than the caller.
 */
StatusOr<std::filesystem::path> ResolveMountPointImpl(
    const std::vector<ProcParser::MountInfo>& mount_infos,
    const std::vector<ProcParser::MountInfo>& root_mount_info,
    const std::filesystem::path& mount_point) {
  std::string device_number;
  std::string device_root;
  for (const auto& mount_info : mount_infos) {
    if (mount_info.mount_point == mount_point.string()) {
      device_number = mount_info.dev;
      device_root = mount_info.root;
      break;
    }
  }
  if (device_number.empty() || device_root.empty()) {
    return error::InvalidArgument("Mount info does not have the requested mount_point=$0",
                                  mount_point.string());
  }

  // Now looks for the mount point of any of the devices' filesystem that can be the parent of
  // the device root path of the requested mount point.
  //
  // For example, assuming the input pid has a MountInfo as follows:
  // {0:1, /foo/bar, /tmp}
  //
  // To look for an accessible mount point for '/tmp', we look for mount points of any one of
  // {/, /foo, /foo/bar} on device 0:1's filesystem. Assuming pid 1 has a MountInfo like this:
  // {0:1, /, /tmp}
  //
  // We can access device 0:1's root through /tmp, and should return /tmp/foo/bar, through which
  // the input pid's '/tmp' can be accessed.
  for (const auto& mount_info : root_mount_info) {
    if (mount_info.dev != device_number) {
      continue;
    }
    auto rel_path_or = fs::GetChildRelPath(device_root, mount_info.root);
    if (!rel_path_or.ok()) {
      continue;
    }
    const std::filesystem::path device_mount_point(mount_info.mount_point);
    return fs::JoinPath({&device_mount_point, &rel_path_or.ValueOrDie()});
  }
  return error::InvalidArgument("Could not find mount point for to device=$0 root=$1",
                                device_number, device_root);
}

}  // namespace

StatusOr<std::unique_ptr<FileSystemResolver>> FileSystemResolver::Create(pid_t pid) {
  system::ProcParser proc_parser(system::Config::GetInstance());

  auto mount_resolver = std::unique_ptr<FileSystemResolver>(new FileSystemResolver());
  PL_RETURN_IF_ERROR(proc_parser.ReadMountInfos(pid, &mount_resolver->mount_infos_));
  PL_RETURN_IF_ERROR(proc_parser.ReadMountInfos(1, &mount_resolver->root_mount_infos_));

  return mount_resolver;
}

StatusOr<std::filesystem::path> FileSystemResolver::ResolveMountPoint(
    const std::filesystem::path& mount_point) {
  return ResolveMountPointImpl(mount_infos_, root_mount_infos_, mount_point);
}

StatusOr<std::filesystem::path> FileSystemResolver::ResolvePath(const std::filesystem::path& path) {
  // Find the longest parent path that is accessible of the file, by resolving mount
  // point starting from the immediate parent through the root.
  for (const fs::PathSplit& path_split : fs::EnumerateParentPaths(path)) {
    auto resolved_mount_path_or = ResolveMountPoint(path_split.parent);
    if (resolved_mount_path_or.ok()) {
      return fs::JoinPath({&resolved_mount_path_or.ValueOrDie(), &path_split.child});
    }
  }

  return error::Internal("Could not resolve $0", path.string());
}

}  // namespace stirling
}  // namespace pl
