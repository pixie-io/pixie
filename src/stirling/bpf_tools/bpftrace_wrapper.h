#pragma once

#ifdef __linux__

#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"

#include "third_party/bpftrace/src/bpforc.h"
#include "third_party/bpftrace/src/bpftrace.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

/**
 * Wrapper around BPFTrace, as a convenience.
 */
class BPFTraceWrapper {
 public:
  ~BPFTraceWrapper() { Stop(); }

  /**
   * Compiles the BPFTrace program and deploys it.
   */
  Status Deploy(std::string_view bpf_program, const std::vector<std::string>& params,
                const std::function<void(const std::vector<bpftrace::Field>, uint8_t*)>&
                    printf_callback = nullptr);

  /**
   * Drains all of the managed perf buffers, calling the handle function for each event.
   */
  void PollPerfBuffers(int timeout_ms = 1);

  /**
   * Stops the running BPFTrace program.
   */
  void Stop();

  bpftrace::BPFTraceMap GetBPFMap(const std::string& name);

 private:
  bpftrace::BPFtrace bpftrace_;
  std::unique_ptr<bpftrace::BpfOrc> bpforc_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
