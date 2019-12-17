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

#ifdef __cplusplus
inline std::string_view TypeName(EventType type) {
  switch (type) {
    case kUnknown:
      return "unknown";
    case kGRPCWriteHeader:
      return "write_header";
    case kGRPCOperateHeaders:
      return "operate_headers";
    case kReadData:
      return "operate_headers";
    case kWriteData:
      return "operate_headers";
    default:
      DCHECK(false);
      // For GCC.
      return "unhandled";
  }
}
#endif

struct header_field_t {
  uint32_t size;
  char msg[HEADER_FIELD_STR_SIZE];
};

enum HeaderEventType { kHeaderEventUnknown, kHeaderEventRead, kHeaderEventWrite };

#ifdef __cplusplus
inline std::string_view HeaderEventTypeName(HeaderEventType type) {
  switch (type) {
    case kHeaderEventUnknown:
      return "unknown";
    case kHeaderEventRead:
      return "header_read";
    case kHeaderEventWrite:
      return "header_write";
    default:
      DCHECK(false);
      // For GCC.
      return "unhandled";
  }
}
#endif

struct go_grpc_http2_header_event_t {
  struct header_attr_t {
    // TODO(oazizi): The type fields below must be reconciled.
    enum EventType type;
    enum HeaderEventType htype;
    uint64_t timestamp_ns;
    struct conn_id_t conn_id;
    uint32_t stream_id;
  } attr;

  struct header_field_t name;
  struct header_field_t value;
};

struct conn_symaddrs_t {
  int64_t syscall_conn;
  int64_t tls_conn;
  int64_t tcp_conn;
};

enum DataFrameEventType { kDataFrameEventUnknown, kDataFrameEventRead, kDataFrameEventWrite };

#ifdef __cplusplus
inline std::string_view DataFrameEventTypeName(DataFrameEventType type) {
  switch (type) {
    case kDataFrameEventUnknown:
      return "unknown";
    case kDataFrameEventRead:
      return "data_read";
    case kDataFrameEventWrite:
      return "data_write";
    default:
      DCHECK(false);
      // For GCC.
      return "unhandled";
  }
}
#endif

struct go_grpc_data_event_t {
  struct data_attr_t {
    // TODO(oazizi): The type fields below must be reconciled.
    enum EventType type;
    enum DataFrameEventType ftype;
    uint64_t timestamp_ns;
    struct conn_id_t conn_id;
    uint32_t stream_id;
    uint32_t data_len;
  } attr;
  char data[MAX_DATA_SIZE];
};
