#pragma once

#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/proc_trace.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"

namespace pl {
namespace stirling {

class ProcTracer : public bpf_tools::BCCWrapper {
 public:
  /**
   * Initialize ProcTracer according to the spec.
   */
  Status Init();

  /**
   * For perf buffer callback to insert events.
   */
  void AcceptProcCreationEvent(const proc_creation_event_t& event);

  /**
   * Polls perf buffer and reads the events.
   */
  std::vector<proc_creation_event_t> ExtractProcCreationEvents();

 private:
  std::vector<proc_creation_event_t> events_;
};

}  // namespace stirling
}  // namespace pl
