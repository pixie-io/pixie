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
  /**
   * Callback from BPFTrace is a raw pointer to data which needs to be
   * interpreted as the printf data.
   */
  using PrintfCallback = std::function<void(uint8_t*)>;

  ~BPFTraceWrapper() { Stop(); }

  /**
   * Compiles the BPFTrace program.
   */
  Status Compile(std::string_view bpf_program, const std::vector<std::string>& params);

  /**
   * Deploys the previously compiled BPFTrace program.
   */
  Status Deploy(const PrintfCallback& printf_callback = nullptr);

  /**
   * Drains all of the managed perf buffers, calling the handle function for each event.
   */
  void PollPerfBuffers(int timeout_ms = 100);

  /**
   * Stops the running BPFTrace program.
   */
  void Stop();

  /**
   * Returns the fields of the BPFTrace program's printf.
   * @return error if there are not exactly one printf statement in the compiled program.
   *         otherwise, returns a vector of fields which represents the types in the printf.
   */
  StatusOr<std::vector<bpftrace::Field>> OutputFields();

  // NOTE: In addition to OutputFields(), it's possible to grab the format string.
  // If we ever need to recreate the printf on the fly, we could add that as an extra column.
  // Potentially useful in collapsing multiple tables into a "log" table.

  /**
   * Gets the specified map name, using BPFTrace name. Map name should include the '@' prefix.
   */
  bpftrace::BPFTraceMap GetBPFMap(const std::string& name);

 private:
  bpftrace::BPFtrace bpftrace_;
  std::unique_ptr<bpftrace::BpfOrc> bpforc_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
