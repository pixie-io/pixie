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
  int64_t syscall_conn;
  int64_t tls_conn;
  int64_t tcp_conn;
};
