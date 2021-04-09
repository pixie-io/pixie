#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/proc_parser.h"

// This file deals with process path resolution.
// In particular, FilePathResolver handles cases when these paths are within containers.

namespace pl {
namespace stirling {

/**
 * Returns the path to the binary of a process specified by /proc/<pid>.
 */
StatusOr<std::filesystem::path> ProcExe(const std::filesystem::path& proc_pid);

/**
 * Returns the path to the binary of a process pid and an optional start time.
 */
StatusOr<std::filesystem::path> ProcExe(uint32_t pid,
                                        std::optional<int64_t> start_time = std::nullopt);

/**
 * Resolves a path from within a pid namespace to the path on the host.
 * Implemented as a class, so the state of creation can be saved for multiple resolutions.
 * Otherwise, parsing /proc becomes very expensive.
 */
class FilePathResolver {
 public:
  static StatusOr<std::unique_ptr<FilePathResolver>> Create(pid_t pid = 1);

  /**
   * Changes the PID for which to resolve paths.
   * This is more efficient than creating a new FilePathResolver for the new PID,
   * since some state can be shared.
   */
  Status SetMountNamespace(pid_t pid);

  /**
   * Given a path which may be in a container, returns the host-resolved path,
   * accounting for any overlay filesystems.
   *
   * For normal processes, this function simply returns the input path, unchanged.
   *
   * For containers which use overlay filesystems, this function returns the location of
   * the path in the container as a host-relative path.
   *
   * Example #1 (simple process): *
   *   ResolvePath("/usr/bin/server") -> /usr/bin/server
   *
   * Example #2 (container)
   *   ResolvePath("/app/server") ->  /var/lib/docker/overlay2/402fe2...be0/merged/app/server
   *
   * @param path The path to resolve, as seen in the mount namespace of the process.
   * @return The host-resolved path.
   */
  StatusOr<std::filesystem::path> ResolvePath(const std::filesystem::path& path);

  /**
   * Given a mount point within the mount namespace of the process (e.g. in a container),
   * returns the host-resolved mount point.
   *
   * Example #1: regular process not in container. Mount is already host-relative.
   *   ResolveMountPoint("/"):   /
   *
   * Example #2: container with an overlay on / (as discovered through /proc/pid/mounts)
   *   ResolveMountPoint("/"):   /var/lib/docker/overlay2/402fe2...be0/merged
   *
   * @param mount_point Mount point within the mount namespace of the process.
   * @return The host-resolved mount point.
   */

  StatusOr<std::filesystem::path> ResolveMountPoint(const std::filesystem::path& mount_point);

 private:
  FilePathResolver() {}

  pid_t pid_ = -1;
  std::vector<system::ProcParser::MountInfo> mount_infos_;
  std::vector<system::ProcParser::MountInfo> root_mount_infos_;
};

/**
 * A wrapper around FilePathResolver that manages a lazy-loaded instance of the resolver.
 *
 * FilePathResolver is a very expensive structure to make, and so this wrapper uses
 * lazy-loading to minimize its cost.
 *
 * In particular, Create() is called just-in-time on the first use, and is
 * cached from that point onwards, until the next call to Refresh().
 *
 * The SetMountNamespace and ResolvePath APIs match that of the FilePathResolver.
 */
class LazyLoadedFPResolver {
 public:
  Status SetMountNamespace(pid_t pid) {
    PL_RETURN_IF_ERROR(LazyLoad());
    return fp_resolver_->SetMountNamespace(pid);
  }

  StatusOr<std::filesystem::path> ResolvePath(const std::filesystem::path& path) {
    PL_RETURN_IF_ERROR(LazyLoad());
    return fp_resolver_->ResolvePath(path);
  }

  void Refresh() {
    // This is lazy-loaded, so to refresh, we need only reset the pointer.
    // The next call to SetMountNamespace/ResolvePath will refresh the state lazily.
    fp_resolver_.reset();
  }

 private:
  Status LazyLoad() {
    if (fp_resolver_ == nullptr) {
      PL_ASSIGN_OR_RETURN(fp_resolver_, FilePathResolver::Create());
    }
    return Status::OK();
  }

  std::unique_ptr<FilePathResolver> fp_resolver_;
};

/**
 * Return the path to the currently running process (i.e. /proc/self/exe).
 * This function will return a host relative path self is in a container.
 */
StatusOr<std::filesystem::path> GetSelfPath();

}  // namespace stirling
}  // namespace pl
