#pragma once

// This file contains definitions that are shared between various kprobes and uprobes.

enum TrafficDirection {
  kEgress,
  kIngress,
};

// Protocol being used on a connection (HTTP, MySQL, etc.).
enum TrafficProtocol {
  kProtocolUnknown,
  kProtocolHTTP,
  kProtocolHTTP2,
  kProtocolMySQL,
  kNumProtocols
};

// The direction of traffic expected on a probe. Values are used in bit masks.
enum ReqRespRole { kRoleUnknown = 1 << 0, kRoleRequestor = 1 << 1, kRoleResponder = 1 << 2 };

struct traffic_class_t {
  // The protocol of traffic on the connection (HTTP, MySQL, etc.).
  enum TrafficProtocol protocol;
  // Classify traffic as requests, responses or mixed.
  enum ReqRespRole role;
};

struct conn_id_t {
  // Comes from the process from which this is captured.
  // See https://stackoverflow.com/a/9306150 for details.
  // Use union to give it two names. We use tgid in kernel-space, pid in user-space.
  union {
    uint32_t tgid;
    uint32_t pid;
  };
  // The start time of the PID, so we can disambiguate PIDs.
  union {
    uint64_t tgid_start_time_ticks;
    uint64_t pid_start_time_ticks;
  };
  // The file descriptor to the opened network connection.
  uint32_t fd;
  // Generation number of the FD (increments on each FD reuse in the TGID).
  uint32_t generation;
};

// TODO(oazizi): Reconcile probe_info_t and conn_id_t.
struct probe_info_t {
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
