#include <fcntl.h>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "src/stirling/utils/kprobe_cleaner.h"

namespace pl {
namespace stirling {
namespace utils {

const char kAttachedProbesFile[] = "/sys/kernel/debug/tracing/kprobe_events";

StatusOr<std::vector<std::string>> SearchForAttachedProbes(std::string_view marker) {
  std::vector<std::string> leaked_probes;

  std::ifstream infile(kAttachedProbesFile);
  if (!infile.good()) {
    return error::Internal("Failed to open file for reading: $0", kAttachedProbesFile);
  }
  std::string line;
  while (std::getline(infile, line)) {
    if (absl::StrContains(line, marker)) {
      std::vector<std::string> split = absl::StrSplit(line, ' ');
      if (split.size() != 2) {
        return error::Internal("Unexpected format when reading file: $0", kAttachedProbesFile);
      }

      std::string probe = std::move(split[0]);

      // Note that a probe looks like the following:
      //     p:kprobes/your_favorite_probe_name_here __x64_sys_connect
      // Perform a quick (but not thorough) sanity check that we have the right format.
      if ((probe[0] != 'p' && probe[0] != 'r') || probe[1] != ':') {
        return error::Internal("Unexpected probe string: $0", probe);
      }

      leaked_probes.push_back(std::move(probe));
    }
  }

  return leaked_probes;
}

Status RemoveProbes(std::vector<std::string> probes) {
  // Unfortunately std::ofstream doesn't properly append to /sys/kernel/debug/tracing/kprobe_events.
  // It appears related to its use of fopen() instead of open().
  // So doing the write in old-school C-style.
  int fd = open(kAttachedProbesFile, O_WRONLY | O_APPEND, 0);
  if (fd < 0) {
    return error::Internal("Failed to open file for writing: $0", kAttachedProbesFile);
  }

  std::vector<std::string> errors;
  for (auto& probe : probes) {
    // Here we modify first character, which is normally 'p' or 'r' to '-'.
    // This indicates that the probe should be removed.
    probe[0] = '-';
    VLOG(1) << absl::Substitute("Writing $0", probe);

    if (write(fd, probe.data(), probe.size()) < 0) {
      return error::Internal("Failed to write to file: $0", kAttachedProbesFile);
    }
    // Note that even if write succeeds, it doesn't confirm that the probe was properly removed.
    // We can only confirm that we wrote to the file.
  }

  close(fd);

  return Status::OK();
}

Status KprobeCleaner(std::string_view marker) {
  LOG(INFO) << absl::Substitute("Cleaning probes with the following marker: $0", marker);

  std::vector<std::string> leaked_probes;
  PL_ASSIGN_OR_RETURN(leaked_probes, SearchForAttachedProbes(marker));
  PL_RETURN_IF_ERROR(RemoveProbes(leaked_probes));

  std::vector<std::string> leaked_probes_after;
  PL_ASSIGN_OR_RETURN(leaked_probes_after, SearchForAttachedProbes(marker));
  if (leaked_probes_after.size() != 0) {
    return error::Internal("Wasn't able to remove all probes. Initial count=$0, final count=$1",
                           leaked_probes.size(), leaked_probes_after.size());
  }

  LOG(INFO) << absl::Substitute("Cleaned up $0 probes", leaked_probes.size());

  return Status::OK();
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
