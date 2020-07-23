#pragma once

#include <string>
#include <vector>

#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/ir/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

constexpr size_t kStructStringSize = 64;

struct BCCProgram {
  struct PerfBufferSpec {
    std::string name;
    ir::physical::Struct output;
  };

  // TODO(yzhao): We probably need kprobe_specs as well.
  std::vector<bpf_tools::UProbeSpec> uprobe_specs;
  std::vector<PerfBufferSpec> perf_buffer_specs;
  std::string code;
};

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
