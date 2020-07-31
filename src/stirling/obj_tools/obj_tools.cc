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

StatusOr<std::filesystem::path> GetActiveBinary(uint32_t pid, std::optional<int64_t> start_time) {
  const std::filesystem::path& host_path = system::Config::GetInstance().host_path();
  const std::filesystem::path& proc_path = system::Config::GetInstance().proc_path();
  std::filesystem::path pid_path = proc_path / std::to_string(pid);
  if (start_time.has_value()) {
    int64_t pid_start_time = system::GetPIDStartTimeTicks(pid_path);
    if (start_time.value() != pid_start_time) {
      return error::NotFound(
          "This is not the pid you are looking for... "
          "Start time does not match (specification: $0 vs system: $1).",
          start_time.value(), pid_start_time);
    }
  }

  return GetActiveBinary(host_path, pid_path);
}

StatusOr<std::filesystem::path> GetActiveBinary(const std::filesystem::path& host_path,
                                                const std::filesystem::path& proc_pid) {
  PL_ASSIGN_OR_RETURN(std::filesystem::path proc_exe, ResolveProcExe(proc_pid));

  // If we're running in a container, convert exe to be relative to our host mount.
  // Note that we mount host '/' to '/host' inside container.
  // Warning: must use JoinPath, because we are dealing with two absolute paths.
  std::filesystem::path host_exe = fs::JoinPath({&host_path, &proc_exe});
  PL_RETURN_IF_ERROR(fs::Exists(host_exe));
  return host_exe;
}

std::map<std::string, std::vector<int>> GetActiveBinaries(
    const std::map<int32_t, std::filesystem::path>& pid_paths,
    const std::filesystem::path& host_path) {
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

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
