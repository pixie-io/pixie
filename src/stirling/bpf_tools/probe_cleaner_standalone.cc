#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/probe_cleaner.h"

// This is the string inserted by our modified BCC into the name
// of all kprobes. We use it to look for leaked probes.
const char kPixieMarker[] = "__pixie__";

DEFINE_string(cleanup_marker, kPixieMarker, "Marker to search for when deleting probes.");

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);

  pl::Status s = pl::stirling::utils::CleanProbes(kPixieMarker);
  LOG_IF(ERROR, !s.ok()) << s.msg();

  pl::ShutdownEnvironmentOrDie();
}
