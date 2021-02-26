#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/probe_cleaner.h"

DEFINE_string(cleanup_marker, ::pl::stirling::utils::kPixieBPFProbeMarker,
              "Marker to search for when deleting probes.");

int main(int argc, char** argv) {
  pl::EnvironmentGuard env_guard(&argc, argv);

  pl::Status s = pl::stirling::utils::CleanProbes(FLAGS_cleanup_marker);
  LOG_IF(ERROR, !s.ok()) << s.msg();

  return 0;
}
