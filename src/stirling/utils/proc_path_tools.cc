#include "src/stirling/utils/proc_path_tools.h"

#include <filesystem>
#include <memory>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/config.h"
#include "src/common/system/proc_parser.h"

using ::pl::system::ProcParser;

namespace pl {
namespace stirling {

StatusOr<std::filesystem::path> ProcExe(const std::filesystem::path& proc_pid) {
  std::filesystem::path proc_exe_path = proc_pid / "exe";

  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, fs::ReadSymlink(proc_exe_path));
  if (proc_exe.empty() || proc_exe == "/") {
    // Not sure what causes this, but sometimes get symlinks that point to "/".
    // Seems to happen with PIDs that are short-lived, because I can never catch it in the act.
    // I suspect there is a race with the proc filesystem, with PID creation/destruction,
    // but this is not confirmed. Would be nice to understand the root cause, but for now, just
    // filter these out.
    return error::Internal("Symlink appears malformed.");
  }

  return proc_exe;
}

StatusOr<std::filesystem::path> ProcExe(uint32_t pid, std::optional<int64_t> start_time) {
  const system::Config& sysconfig = system::Config::GetInstance();
  std::filesystem::path proc_pid = sysconfig.proc_path() / std::to_string(pid);

  if (start_time.has_value()) {
    StatusOr<int64_t> pid_start_time = system::GetPIDStartTimeTicks(proc_pid);
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

  return ProcExe(proc_pid);
}

namespace {

/**
 * Maps the given mount_point within a process to a mount point within the host.
 * These mount points can be different if the process is a container.
 *
 * @param mount_infos Mount info for the process, which may be in a namespace.
 * @param root_mount_infos Mount info for the host.
 * @param mount_point The mount point to resolve.
 * @return The mount point resolved to the host.
 */
StatusOr<std::filesystem::path> ResolveMountPointImpl(
    const std::vector<ProcParser::MountInfo>& mount_infos,
    const std::vector<ProcParser::MountInfo>& root_mount_infos,
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
  for (const auto& mount_info : root_mount_infos) {
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

StatusOr<std::unique_ptr<FilePathResolver>> FilePathResolver::Create(pid_t pid) {
  system::ProcParser proc_parser(system::Config::GetInstance());

  auto fp_resolver = std::unique_ptr<FilePathResolver>(new FilePathResolver());
  PL_RETURN_IF_ERROR(proc_parser.ReadMountInfos(1, &fp_resolver->root_mount_infos_));

  if (pid == 1) {
    fp_resolver->mount_infos_ = fp_resolver->root_mount_infos_;
  } else {
    PL_RETURN_IF_ERROR(fp_resolver->SetMountNamespace(pid));
  }

  return fp_resolver;
}

Status FilePathResolver::SetMountNamespace(pid_t pid) {
  system::ProcParser proc_parser(system::Config::GetInstance());

  if (pid_ == pid) {
    return Status::OK();
  }

  // Set to -1 in case ReadMountInfos() fails; otherwise we'd be in a weird state.
  pid_ = -1;
  mount_infos_.clear();
  PL_RETURN_IF_ERROR(proc_parser.ReadMountInfos(pid, &mount_infos_));
  pid_ = pid;

  return Status::OK();
}

StatusOr<std::filesystem::path> FilePathResolver::ResolveMountPoint(
    const std::filesystem::path& mount_point) {
  return ResolveMountPointImpl(mount_infos_, root_mount_infos_, mount_point);
}

StatusOr<std::filesystem::path> FilePathResolver::ResolvePath(const std::filesystem::path& path) {
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

StatusOr<std::filesystem::path> GetSelfPath() {
  const system::Config& sysconfig = system::Config::GetInstance();
  ::pl::system::ProcParser proc_parser(sysconfig);
  PL_ASSIGN_OR_RETURN(std::filesystem::path self_path, proc_parser.GetExePath(getpid()));
  PL_ASSIGN_OR_RETURN(std::unique_ptr<FilePathResolver> fp_resolver,
                      FilePathResolver::Create(getpid()));
  PL_ASSIGN_OR_RETURN(self_path, fp_resolver->ResolvePath(self_path));
  return sysconfig.ToHostPath(self_path);
}

}  // namespace stirling
}  // namespace pl
