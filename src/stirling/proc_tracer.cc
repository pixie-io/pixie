#include "src/stirling/proc_tracer.h"

#include <utility>
#include <vector>

#include "src/stirling/bcc_bpf_interface/proc_trace.h"

BCC_SRC_STRVIEW(proc_trace_bcc_script, proc_trace);

namespace pl {
namespace stirling {

namespace {

void HandleProcCreationEvent(void* cb_cookie, void* data, int /*data_size*/) {
  DCHECK(cb_cookie != nullptr) << "Perf buffer callback not set-up properly. Missing cb_cookie.";
  auto* tracer = static_cast<ProcTracer*>(cb_cookie);
  struct proc_creation_event_t event;
  memcpy(&event, data, sizeof(event));
  tracer->AcceptProcCreationEvent(event);
}

void HandleProcCreationEventLoss(void* /*cb_cookie*/, uint64_t lost) {
  LOG(WARNING) << absl::Substitute("proc_creation_events lost $0 samples.", lost);
}

}  // namespace

constexpr auto kKProbeSpecs = MakeArray<bpf_tools::KProbeSpec>({
    {"clone", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_clone"},
    {"vfork", bpf_tools::BPFProbeAttachType::kReturn, "syscall__probe_ret_vfork"},
});

constexpr char kProcCreationEventsName[] = "proc_creation_events";
const auto kPerfBufferSpecs = MakeArray<bpf_tools::PerfBufferSpec>({
    {kProcCreationEventsName, HandleProcCreationEvent, HandleProcCreationEventLoss},
});

Status ProcTracer::Init() {
  PL_RETURN_IF_ERROR(InitBPFProgram(proc_trace_bcc_script));
  PL_RETURN_IF_ERROR(OpenPerfBuffers(kPerfBufferSpecs, /*cb_cookie*/ this));
  PL_RETURN_IF_ERROR(AttachKProbes(kKProbeSpecs));
  return Status::OK();
}

void ProcTracer::AcceptProcCreationEvent(const proc_creation_event_t& event) {
  events_.push_back(event);
}

std::vector<proc_creation_event_t> ProcTracer::ExtractProcCreationEvents() {
  PollPerfBuffers(/*timeout_ms*/ 10);
  return std::move(events_);
}

}  // namespace stirling
}  // namespace pl
