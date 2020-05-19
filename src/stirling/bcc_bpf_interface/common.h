#pragma once

#ifdef __cplusplus
#include <absl/strings/substitute.h>
#include <string>
#endif

// This file contains definitions that are shared between various kprobes and uprobes.

enum MessageType { kUnknown, kRequest, kResponse };

enum TrafficDirection {
  kEgress,
  kIngress,
};

// Protocol being used on a connection (HTTP, MySQL, etc.).
enum TrafficProtocol {
  kProtocolUnknown = 0,
  kProtocolHTTP,
  // TODO(oazizi): Consolidate the two HTTP2 protocols once Uprobe work is complete.
  // Currently BPF doesn't produce kProtocolHTTP2Uprobe, so it is only created through tests.
  kProtocolHTTP2,
  kProtocolHTTP2Uprobe,
  kProtocolMySQL,
  kProtocolCQL,
  kProtocolPGSQL,
  kNumProtocols
};

// The direction of traffic expected on a probe. Values are used in bit masks.
enum EndpointRole {
  kRoleUnknown = 0,
  kRoleClient = 1 << 0,
  kRoleServer = 1 << 1,
  kRoleAll = kRoleClient | kRoleServer,
};

struct traffic_class_t {
  // The protocol of traffic on the connection (HTTP, MySQL, etc.).
  enum TrafficProtocol protocol;
  // Classify traffic as requests, responses or mixed.
  enum EndpointRole role;
};

// UPID stands for unique pid.
// Since PIDs can be reused, this attaches the start time of the PID,
// so that the identifier becomes unique.
// Note that this version is node specific; there is also a 'class UPID'
// definition under shared which also includes an Agent ID (ASID),
// to uniquely identify PIDs across a cluster. The ASID is not required here.
struct upid_t {
  // Comes from the process from which this is captured.
  // See https://stackoverflow.com/a/9306150 for details.
  // Use union to give it two names. We use tgid in kernel-space, pid in user-space.
  union {
    uint32_t pid;
    uint32_t tgid;
  };
  uint64_t start_time_ticks;
};

struct conn_id_t {
  // The unique identifier of the pid/tgid.
  struct upid_t upid;
  // The file descriptor to the opened network connection.
  uint32_t fd;
  // Unique id of the conn_id (timestamp).
  uint64_t tsid;
};

#ifdef __cplusplus
inline std::string ToString(const conn_id_t& conn_id) {
  return absl::Substitute("[pid=$0 start_time_ticks=$1 fd=$2 gen=$3]", conn_id.upid.pid,
                          conn_id.upid.start_time_ticks, conn_id.fd, conn_id.tsid);
}
#endif

// Specifies the corresponding indexes of the entries of a per-cpu array.
enum ControlValueIndex {
  // This specify one pid to monitor. This is used during test to eliminate noise.
  // TODO(yzhao): We need a more robust mechanism for production use, which should be able to:
  // * Specify multiple pids up to a certain limit, let's say 1024.
  // * Support efficient lookup inside bpf to minimize overhead.
  kTargetTGIDIndex = 0,
  kStirlingTGIDIndex,
  kNumControlValues,
};

struct conn_symaddrs_t {
  // Conn interface types.
  int64_t syscall_conn;
  int64_t tls_conn;
  int64_t tcp_conn;

  // Struct member offsets.
  // Naming maintains golang style: <struct>_<member>_offset

  // TODO(oazizi): Remove offset in comments before submitting.

  // Members of google.golang.org/grpc/internal/transport.http2Server.
  int32_t http2Server_conn_offset;  // 16 or 24

  // Members of google.golang.org/grpc/internal/transport.http2Client.
  int32_t http2Client_conn_offset;  // 64

  // Members of google.golang.org/grpc/internal/transport.loopyWriter.
  int32_t loopyWriter_framer_offset;  // 40

  // Members of golang.org/x/net/net/http2.Framer.
  int32_t Framer_w_offset;  // 112

  // Members of golang.org/x/net/http2.MetaHeadersFrame.
  int32_t MetaHeadersFrame_HeadersFrame_offset;  // 0
  int32_t MetaHeadersFrame_Fields_offset;        // 0

  // Members of golang.org/x/net/http2.HeadersFrame.
  int32_t HeadersFrame_FrameHeader_offset;  // 0

  // Members of golang.org/x/net/http2.FrameHeader.
  int32_t FrameHeader_Type_offset;      // 1
  int32_t FrameHeader_Flags_offset;     // 2
  int32_t FrameHeader_StreamID_offset;  // 8

  // Members of golang.org/x/net/http2.DataFrame.
  int32_t DataFrame_data_offset;  // 16

  // Members of google.golang.org/grpc/internal/transport.bufWriter.
  int32_t bufWriter_conn_offset;  // 40

  // Members of net/http.http2serverConn.
  int32_t http2serverConn_conn_offset;          // 16
  int32_t http2serverConn_hpackEncoder_offset;  // 360

  // Members of net/http.http2HeadersFrame
  int32_t http2HeadersFrame_http2FrameHeader_offset;  // 0

  // Members of net/http.http2FrameHeader.
  int32_t http2FrameHeader_Type_offset;      // 1
  int32_t http2FrameHeader_Flags_offset;     // 2
  int32_t http2FrameHeader_StreamID_offset;  // 8

  // Members of golang.org/x/net/http2.DataFrame.
  int32_t http2DataFrame_data_offset;  // 16

  // Members of net/http.http2writeResHeaders.
  int32_t http2writeResHeaders_streamID_offset;   // 0
  int32_t http2writeResHeaders_endStream_offset;  // 48

  // Members of net/http.http2MetaHeadersFrame.
  int32_t http2MetaHeadersFrame_http2HeadersFrame_offset;  // 0;
  int32_t http2MetaHeadersFrame_Fields_offset;             // 8

  // Members of net/http.http2Framer.
  int32_t http2Framer_w_offset;  // 112

  // Members of net/http.http2bufferedWriter
  int32_t http2bufferedWriter_w_offset;  // 0
};
