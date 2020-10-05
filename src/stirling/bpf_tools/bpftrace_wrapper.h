#pragma once

#ifdef __linux__

#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"

#include "third_party/bpftrace/src/bpforc.h"
#include "third_party/bpftrace/src/bpftrace.h"
#include "third_party/bpftrace/src/driver.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

// Dump the bpftrace program's syntax data and other relevant internal process data.
std::string DumpDriver(const bpftrace::Driver& driver);

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
   * @return error if there is no printf statement in the compiled program,
   *         or if all the printfs in the program do not have a consistent format string.
   *         otherwise, returns a vector of fields which represents the types in the printf.
   */
  StatusOr<std::vector<bpftrace::Field>> OutputFields() const;

  /**
   * Returns the format string of the BPFTrace program's printf.
   * @return error if there is no printf statement in the compiled program,
   *         or if all the printfs in the program do not have a consistent format string.
   */
  StatusOr<std::string_view> OutputFmtStr() const;

  /**
   * Gets the specified map name, using BPFTrace name. Map name should include the '@' prefix.
   */
  bpftrace::BPFTraceMap GetBPFMap(const std::string& name);

 protected:
  // Checks the output for dynamic tracing:
  //  1) There must be at least one printf.
  //  2) If there is more than one printf, all printfs have a consistent format string.
  //     In other words, all printfs must output to the same table.
  Status CheckPrintfs() const;

  bpftrace::BPFtrace bpftrace_;
  std::unique_ptr<bpftrace::BpfOrc> bpforc_;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
