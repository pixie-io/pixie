#pragma once

#include <experimental/filesystem>
#include <map>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/common.h"

namespace pl {
namespace stirling {
namespace obj_tools {

/**
 * ResolveExe takes a /proc/<pid> directory, and resolve the binary for that process.
 * It accounts for any overlay filesystems to resolve the exe to its actual location.
 * This is important for exe files in containers, where the file is actually located
 * on the host at some other location.
 *
 * NOTE: Today this function is not robust and looks for a very specific pattern.
 * IT IS NOT PRODUCTION READY.
 *
 * @param proc_pid Path to process info /proc/<pid>.
 * @return The resolved path. Either the original exe symlink if no overlay fs was found, or the
 * path to the host location if an overlay was found.
 */
pl::StatusOr<std::experimental::filesystem::path> ResolveExe(
    std::experimental::filesystem::path proc_pid);

/**
 * GetActiveBinaries searches the /proc filesystem to collect all active binaries.
 * Essentially it returns the files pointed to by /proc/<pid>/exe, for all <pids>.
 *
 * @param proc Path to the proc filesystem (typically should be "/proc")
 * @return a set of all active binaries.
 */
std::map<std::string, std::vector<int>> GetActiveBinaries(std::experimental::filesystem::path proc);

/**
 * Looks up specific symbols of the binaries, and returns a map from PIDs that execute the
 * binaries to the symbol addresses.
 */
std::map<uint32_t, struct conn_symaddrs_t> GetSymAddrs(
    const std::map<std::string, std::vector<int>>& binaries);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
