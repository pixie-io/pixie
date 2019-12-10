#include "src/stirling/bcc_bpf_interface/go_grpc_types.h"

BPF_PERF_OUTPUT(go_grpc_header_events);

// The target function:
// https://github.com/grpc/grpc-go/blob/4b2104f1fb2b5f1a267fab0a7e704a1066d53775/internal/transport/controlbuf.go#L654
int dummy_uprobe(struct pt_regs* ctx) {
  bpf_trace_printk("dummy_uprobe()\n");
  struct go_grpc_http2_header_event_t event;
  event.value = 12345;
  go_grpc_header_events.perf_submit(ctx, &event, sizeof(event));
  return 0;
}
