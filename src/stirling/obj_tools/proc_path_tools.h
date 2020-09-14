#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/common.h"

namespace pl {
namespace stirling {
namespace obj_tools {

// This file deals with process path resolution, handling cases when these paths are within
// containers.

/**
 * Given a pid, returns a host path that represents the root directory within the
 * context of the process. Paths within the process should be prefixed with the
 * result of this function to get a host-relative path.
 *
 * For normal processes, this function simply returns '', which indicates that any
 * paths in the process do not need to be prefixed with anything to be valid.
 *
 * For containers which use overlay filesystems, the root directory in the process
 * refers to a different path on the host, which this then function returns. By
 * prefixing the container paths with the result of this function, one has a
 * host-relative path.
 *
 * Example #1 (simple process): Returns an empty path, since there is no overlay.
 *   ResolveProcessRootDir():   <empty>
 *
 * Example #2 (container): Returns the location of the process root dir on the host.
 *   ResolveProcessRootDir():   /var/lib/docker/overlay2/402fe2...be0/merged
 *
 * WARNING: Today this function is not robust and looks for a very specific pattern.
 * It only works on overlayfs, and makes assumptions about the ordering of mounts in
 * /proc/<pid>/mounts.
 *
 * @param proc_pid Process for which the root directory is desired.
 * @return The host-resolved path.
 */
pl::StatusOr<std::filesystem::path> ResolveProcessRootDir(const std::filesystem::path& proc_pid);

/**
 * Given a pid, and path within the context of that pid, returns the host-resolved
 * path, accounting for an overlay filesystems.
 *
 * For normal processes, this function simply returns the input path, unchanged.
 *
 * For containers which use overlay filesystems, this function returns the location of
 * the path in the container as a host-relative path.
 *
 * Example #1 (simple process): *
 *   ResolveProcessPath("/usr/bin/server") -> /usr/bin/server
 *
 * Example #2 (container)
 *   ResolveProcessPath("/app/server") ->  /var/lib/docker/overlay2/402fe2...be0/merged/app/server
 *
 * WARNING: Today this function is not robust and looks for a very specific pattern.
 * It only works on overlayfs, and makes assumptions about the ordering of mounts in
 * /proc/<pid>/mounts.
 *
 * @param proc_pid Process for which the root directory is desired.
 * @return The host-resolved path.
 */
pl::StatusOr<std::filesystem::path> ResolveProcessPath(const std::filesystem::path& proc_pid,
                                                       const std::filesystem::path& path);

/**
 * ResolveProcExe takes a /proc/<pid> directory, and resolves the binary path for that process.
 * It accounts for any overlay filesystems to resolve the exe to its actual location.
 * This is important for exe files in containers, where the file is actually located on the host
 * at some other location.
 *
 * @param proc_pid Path to process info /proc/<pid>.
 * @return The resolved path. Either the original exe symlink if no overlay fs was found, or the
 * path to the host location if an overlay was found.
 */
pl::StatusOr<std::filesystem::path> ResolveProcExe(const std::filesystem::path& proc_pid);

/**
 * Returns the path to the executable of the process specified by the pid.
 * Uses system::Config to find proc_path.
 * Returns an error if the resolved file path is not valid.
 */
pl::StatusOr<std::filesystem::path> ResolvePIDBinary(
    uint32_t pid, std::optional<int64_t> start_time = std::nullopt);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
