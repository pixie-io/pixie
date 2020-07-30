#pragma once

#include <string>
#include <vector>

// This depends on LLVM, which has conflicting symbols with ElfReader.
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace pl {
namespace stirling {
namespace dynamic_tracing {

constexpr size_t kStructStringSize = 32;
constexpr size_t kStructByteArraySize = 64;

struct BCCProgram {
  struct PerfBufferSpec {
    std::string name;
    ir::physical::Struct output;

    std::string ToString() const {
      return absl::Substitute("[name=$0 Output struct=$1]", name, output.DebugString());
    }
  };

  // TODO(yzhao): We probably need kprobe_specs as well.
  std::vector<bpf_tools::UProbeSpec> uprobe_specs;
  std::vector<PerfBufferSpec> perf_buffer_specs;
  std::string code;

  std::string ToString() const {
    std::string txt;

    for (const auto& spec : uprobe_specs) {
      absl::StrAppend(&txt, spec.ToString(), "\n");
    }
    for (const auto& spec : perf_buffer_specs) {
      absl::StrAppend(&txt, spec.ToString(), "\n");
    }
    absl::StrAppend(&txt, "[BCC BEGIN]\n", code, "\n[BCC END]");

    return txt;
  }
};

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace pl
