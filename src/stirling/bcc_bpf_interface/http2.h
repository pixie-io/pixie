#pragma once

#include "src/stirling/bcc_bpf_interface/socket_trace.h"

// Macros pleases formatter. Directly use _Pragma() triggers weird reformatting.
#define CLANG_SUPPRESS_WARNINGS_START() \
  _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Weverything\"")

#define CLANG_SUPPRESS_WARNINGS_END() _Pragma("clang diagnostic pop")

const int kHTTP2FrameHeaderSizeInBytes = 9;

static __inline uint32_t bpf_ntohl_chars(const char* buf) {
  uint32_t res = 0;
  bpf_probe_read(&res, sizeof(uint32_t), buf);
  return bpf_ntohl(res);
}

// Parses buf as length-prefixed frames, and updates bookkeeping parameters in conn_info.
static __inline void update_http2_frame_offset(enum TrafficDirection direction, const char* buf,
                                               size_t buf_size, struct conn_info_t* conn_info) {
  uint64_t buf_offset = 0;
  switch (direction) {
    case kEgress:
      buf_offset = conn_info->wr_next_http2_frame_offset;
      break;
    case kIngress:
      buf_offset = conn_info->rd_next_http2_frame_offset;
      break;
  }
  // TODO(yzhao): This will be revised to the maximal value that results into instruction count
  // being smaller than the BPF limit per kprobe:
  // https://docs.cilium.io/en/v1.4/bpf/#instruction-set.
  const int kLoopCount = 100;

  // Avoid warning in GCC and unrolling failure in opt + UI build.
#if defined(__clang__)
  CLANG_SUPPRESS_WARNINGS_START()
#pragma unroll
  CLANG_SUPPRESS_WARNINGS_END()
#endif
  for (int i = 0; i < kLoopCount && buf_offset < buf_size; ++i) {
    // See https://stackoverflow.com/a/7184905/11871800
    // It states bit shift always assumes big-endian. Right-shift to remove the last unused byte for
    // uint32_t.
    uint32_t frame_length = bpf_ntohl_chars(buf + buf_offset) >> 8;
    buf_offset += kHTTP2FrameHeaderSizeInBytes;
    buf_offset += frame_length;
  }
  switch (direction) {
    case kEgress:
      conn_info->wr_next_http2_frame_offset = buf_offset - buf_size;
      break;
    case kIngress:
      conn_info->rd_next_http2_frame_offset = buf_offset - buf_size;
      break;
  }
}
