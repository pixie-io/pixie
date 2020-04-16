#pragma once

#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/bcc_bpf_interface/go_types.h"

// Must be a power of two, otherwise masking will break.
#define HEADER_FIELD_STR_SIZE 128
#define MAX_DATA_SIZE 16384

// These checks are here for compatibility with BPF_LEN_CAP.
#ifdef __cplusplus
static_assert((HEADER_FIELD_STR_SIZE & (HEADER_FIELD_STR_SIZE - 1)) == 0,
              "HEADER_FIELD_STR_SIZE must be a power of 2.");
static_assert((MAX_DATA_SIZE & (MAX_DATA_SIZE - 1)) == 0, "MAX_DATA_SIZE must be a power of 2.");
#endif

// TODO(yzhao): Consider follow C naming conventions.

struct header_field_t {
  uint32_t size;
  char msg[HEADER_FIELD_STR_SIZE];
  // IMPORTANT: This unused byte is required to follow char msg[HEADER_FIELD_STR_SIZE].
  // It is placed here because of the way bpf_probe_read is used.
  // Since bpf_probe_read size must be greater than 0 in 4.14 kernels,
  // we always add 1 to the size.
  // That could cause an overflow in the copy, which this unused byte will absorb.
  char unused[1];
};

enum HeaderEventType { kHeaderEventUnknown, kHeaderEventRead, kHeaderEventWrite };

struct go_grpc_http2_header_event_t {
  struct header_attr_t {
    enum HeaderEventType type;
    uint64_t timestamp_ns;
    struct conn_id_t conn_id;
    uint32_t stream_id;
    bool end_stream;
  } attr;

  struct header_field_t name;
  struct header_field_t value;
};

enum DataFrameEventType { kDataFrameEventUnknown, kDataFrameEventRead, kDataFrameEventWrite };

struct go_grpc_data_event_t {
  struct data_attr_t {
    enum DataFrameEventType type;
    uint64_t timestamp_ns;
    struct conn_id_t conn_id;
    uint32_t stream_id;
    bool end_stream;
    uint32_t data_len;
  } attr;
  char data[MAX_DATA_SIZE];
  // IMPORTANT: This unused byte is required to follow char data[MAX_DATA_SIZE].
  // It is placed here because of the way bpf_probe_read is used.
  // Since bpf_probe_read size must be greater than 0 in 4.14 kernels,
  // we always add 1 to the size.
  // That could cause an overflow in the copy, which this unused byte will absorb.
  char unused[1];
};
