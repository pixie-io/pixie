#pragma once

#include <linux/string.h>
#include <uapi/linux/ptrace.h>

BPF_PERF_OUTPUT(log_events);
BPF_PERCPU_ARRAY(log_event_buf, struct log_event_t, 1);

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define LOC __FILE__ ":" STR(__LINE__) "] "

#define BPF_FN static inline __attribute__((__always_inline__))

BPF_FN void log_text(struct pt_regs* ctx, const char* text) {
  int kZero = 0;
  struct log_event_t* event = log_event_buf.lookup(&kZero);
  if (event == NULL) {
    return;
  }
  const size_t text_size = strlen(text);
  const size_t buf_size = text_size < sizeof(event->msg) ? text_size : sizeof(event->msg);
  event->attr.msg_size = buf_size;
  strncpy(event->msg, text, buf_size);

  log_events.perf_submit(ctx, event, sizeof(event->attr) + buf_size);
}

// Note that pl_bpf_cc_resource() replaces the include with the content of this header.
// So the line number will be different from the raw source code. Anyhow, the line number still
// provides a rough idea on where each logging statement is.
#ifdef NDEBUG
#define DLOG_TEXT(ctx, text)
#else
#define DLOG_TEXT(ctx, text) log_text(ctx, LOC text)
#endif
