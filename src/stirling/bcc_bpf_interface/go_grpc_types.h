#pragma once

#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/bcc_bpf_interface/go_types.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"

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

enum EventType {
  kUnknown,
  kGRPCWriteHeader,
  kGRPCOperateHeaders,
  kReadData,
  kWriteData,
};

struct ProbeInfo {
  union {
    uint32_t tgid;
    uint32_t pid;
  };
  union {
    uint64_t tgid_start_time_ticks;
    uint64_t pid_start_time_ticks;
  };
  uint32_t tid;
  uint64_t timestamp_ns;
};

struct header_field_t {
  uint32_t size;
  char msg[HEADER_FIELD_STR_SIZE];
};

struct go_grpc_http2_header_event_t {
  enum EventType type;
  struct probe_info_t entry_probe;
  int32_t fd;
  uint32_t stream_id;
  struct header_field_t name;
  struct header_field_t value;
};

struct conn_symaddrs_t {
  int64_t syscall_conn;
  int64_t tls_conn;
  int64_t tcp_conn;
};

enum DataFrameEventType { kDataFrameEventUnknown, kDataFrameEventRead, kDataFrameEventWrite };

struct go_grpc_data_event_t {
  struct data_attr_t {
    enum EventType type;
    struct probe_info_t entry_probe;
    uint32_t fd;
    uint32_t generation;
    uint32_t stream_id;
    uint32_t data_len;

    // TODO(oazizi): The fields below must be reconciled with the fields above.
    //---------------------
    uint64_t timestamp_ns;
    struct conn_id_t conn_id;
    struct traffic_class_t traffic_class;
    enum DataFrameEventType ftype;
    //---------------------
  } attr;
  char data[MAX_DATA_SIZE];
};
