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

/**
 * Returns a path to the executable of the process specified by proc_pid.
 */
pl::StatusOr<std::filesystem::path> GetActiveBinary(std::filesystem::path host_path,
                                                    std::filesystem::path proc_pid);

/**
 * GetActiveBinaries searches the /proc filesystem to collect all active binaries.
 * Essentially it returns the files pointed to by /proc/<pid>/exe, for all <pids>.
 *
 * @param proc Path to the proc filesystem (typically should be "/proc")
 * @return a set of all active binaries.
 */
std::map<std::string, std::vector<int>> GetActiveBinaries(
    const std::filesystem::path& host_path,
    const std::map<int32_t, std::filesystem::path>& pid_paths);

/**
 * Looks up specific symbols of the binaries, and returns a map from PIDs that execute the
 * binaries to the symbol addresses.
 */
std::map<uint32_t, struct conn_symaddrs_t> GetSymAddrs(
    const std::map<std::string, std::vector<int>>& binaries);

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
