#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/probe_cleaner.h"

DEFINE_string(cleanup_marker, ::px::stirling::utils::kPixieBPFProbeMarker,
              "Marker to search for when deleting probes.");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);

  px::Status s = px::stirling::utils::CleanProbes(FLAGS_cleanup_marker);
  LOG_IF(ERROR, !s.ok()) << s.msg();

  return 0;
}
