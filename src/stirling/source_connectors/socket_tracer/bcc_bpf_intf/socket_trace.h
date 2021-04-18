/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <linux/in6.h>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

#define AF_UNKNOWN 0xff

const char kControlMapName[] = "control_map";
const uint64_t kSocketTraceNothing = 0;

const int64_t kTraceAllTGIDs = -1;
const char kControlValuesArrayName[] = "control_values";

// Note: A value of 100 results in >4096 BPF instructions, which is too much for older kernels.
#define CONN_CLEANUP_ITERS 90
const int kMaxConnMapCleanupItems = CONN_CLEANUP_ITERS;

// TODO(yzhao): Investigate the performance cost of misaligned memory access (8 vs. 4 bytes).

// This struct contains information collected when a connection is established,
// via an accept() syscall.
struct conn_info_t {
  // Connection identifier (PID, FD, etc.).
  struct conn_id_t conn_id;

  // IP address of the remote endpoint.
  struct sockaddr_in6 addr;

  // The protocol and message type of traffic on the connection (HTTP/Req, HTTP/Resp, MySQL/Req,
  // etc.).
  struct traffic_class_t traffic_class;

  // Whether the connection uses SSL.
  bool ssl;

  // TODO(yzhao): Following fields are only internal tracking only, and is not needed when
  // submitting a new connection. Consider separate these data with the above data that is pushed to
  // perf buffer.

  // The number of bytes written on this connection.
  uint64_t wr_bytes;
  // The number of bytes read on this connection.
  uint64_t rd_bytes;

  // The number of bytes written by application (for uprobe) on this connection.
  uint64_t app_wr_bytes;
  // The number of bytes read by application (for uprobe) on this connection.
  uint64_t app_rd_bytes;

  // Some stats for protocol inference. Used for threshold-based filtering.
  //
  // How many times the data segments have been classified as the designated protocol.
  int32_t protocol_match_count;
  // How many times traffic inference has been applied on this connection.
  int32_t protocol_total_count;
};

// This struct is a subset of conn_info_t. It is used to communicate connect/accept events.
// See conn_info_t for descriptions of the members.
struct conn_event_t {
  uint64_t timestamp_ns;     // Must be shared with close_event_t.
  struct conn_id_t conn_id;  // Must be shared with close_event_t.
  struct sockaddr_in6 addr;
  enum EndpointRole role;
};

// This struct is a subset of conn_info_t. It is used to communicate close events.
// See conn_info_t for descriptions of the members.
struct close_event_t {
  uint64_t timestamp_ns;     // Must be shared with conn_event_t.
  struct conn_id_t conn_id;  // Must be shared with conn_event_t.

  // The number of bytes written and read at time of close.
  uint64_t wr_bytes;
  uint64_t rd_bytes;
};

// Data buffer message size. BPF can submit at most this amount of data to a perf buffer.
//
// NOTE: This size does not directly affect the size of perf buffer submits, as the actual data
// submitted to perf buffers are determined by attr.msg_size. In cases where socket_data_event_t
// is defined as stack variable, the size can be problematic. Currently we only have a few instances
// in *_test.cc files.
//
// TODO(yzhao): We do not yet have a good sense of the desired size. Things to consider:
// * Overhead. This single instance is small. However, we should consider this in the context of all
// possible overhead in BPF program.
// * Complexity. If this buffer is not sufficiently large. We'll need to handle chunked message
// inside user space parsing code.
// ATM, we saw in one case, when gRPC reflection RPC itself is invoked, it can send one
// FileDescriptorProto [1], which often become large. That's the only data point we have right now.
//
// NOTES:
// * Kernel size limit is 32KiB. See https://github.com/iovisor/bcc/issues/2519 for more details.
//
// [1] https://github.com/grpc/grpc-go/blob/master/reflection/serverreflection.go
#define MAX_MSG_SIZE 30720  // 30KiB

// This defines how many chunks a perf_submit can support.
// This applies to messages that are over MAX_MSG_SIZE,
// and effectively makes the maximum message size to be CHUNK_LIMIT*MAX_MSG_SIZE.
#define CHUNK_LIMIT 4

// Unique ID to all syscalls and a few other notable functions.
// This applies to all data events sent to socket_data_events perf buffer.
enum source_function_t {
  kSourceFunctionUnknown,

  // For syscalls.
  kSyscallWrite,
  kSyscallRead,
  kSyscallSend,
  kSyscallRecv,
  kSyscallSendTo,
  kSyscallRecvFrom,
  kSyscallSendMsg,
  kSyscallRecvMsg,
  kSyscallSendMMsg,
  kSyscallRecvMMsg,
  kSyscallWriteV,
  kSyscallReadV,

  // For Go TLS libraries.
  kGoTLSConnWrite,
  kGoTLSConnRead,

  // For SSL libraries.
  kSSLWrite,
  kSSLRead,
};

struct socket_data_event_t {
  // We split attributes into a separate struct, because BPF gets upset if you do lots of
  // size arithmetic. This makes it so that it's attributes followed by message.
  //
  // TODO(yzhao): When adding http2_frame_offset, use a union, so that it allows adding similar data
  // for other protocols.
  struct attr_t {
    // The timestamp when syscall completed (return probe was triggered).
    uint64_t timestamp_ns;
    // Connection identifier (PID, FD, etc.).
    struct conn_id_t conn_id;
    // The protocol on the connection (HTTP, MySQL, etc.), and the server-client role.
    struct traffic_class_t traffic_class;
    // The type of the actual data that the msg field encodes, which is used by the caller
    // to determine how to interpret the data.
    enum TrafficDirection direction;
    // Whether the traffic was collected from an encrypted channel.
    bool ssl;
    // Represents the syscall or function that produces this event.
    enum source_function_t source_fn;
    // A 0-based position number for this event on the connection, in terms of byte position.
    // The position is for the first byte of this message.
    // Note that write/send have separate sequences than read/recv.
    uint64_t pos;
    // The size of the original message. We use this to truncate msg field to minimize the amount
    // of data being transferred.
    uint32_t msg_size;
    // The amount of data actually being sent to user space. This may be less than msg_size if
    // data had to be truncated, or if the data was stripped because we only want to send metadata
    // (e.g. if the connection data tracking has been disabled).
    uint32_t msg_buf_size;
  } attr;
  char msg[MAX_MSG_SIZE];
  // IMPORTANT: This extra byte must follow char msg[MAX_MSG_SIZE].
  // This is because we copy an extra byte during the bpf_probe_read in in perf_submit_buf(),
  // which is, in turn, required to please the BPF verifier 4.14 kernels.
  // This extra byte receives the extra garbage and prevent us from clobbering other memory.
  char unused[1];
};

typedef enum {
  kConnOpen,
  kConnClose,
} ControlEventType;

struct socket_control_event_t {
  ControlEventType type;
  union {
    struct conn_event_t open;
    struct close_event_t close;
  };
};

#ifdef __cplusplus

#include <string>

#include "src/common/base/base.h"

inline std::string ToString(const traffic_class_t& tcls) {
  return absl::Substitute("[protocol=$0 role=$1]", magic_enum::enum_name(tcls.protocol),
                          magic_enum::enum_name(tcls.role));
}

inline std::string ToString(const socket_data_event_t::attr_t& attr) {
  return absl::Substitute(
      "[ts=$0 conn_id=$1 tcls=$2 dir=$3 ssl=$4 source_fn=$5 pos=$6 size=$7 buf_size=$8]",
      attr.timestamp_ns, ToString(attr.conn_id), ToString(attr.traffic_class),
      magic_enum::enum_name(attr.direction), attr.ssl, magic_enum::enum_name(attr.source_fn),
      attr.pos, attr.msg_size, attr.msg_buf_size);
}

inline std::string ToString(const close_event_t& event) {
  return absl::Substitute("[ts=$0 conn_id=$1 wr_bytes=$2 rd_bytes=$3]", event.timestamp_ns,
                          ToString(event.conn_id), event.wr_bytes, event.rd_bytes);
}

inline std::string ToString(const conn_event_t& event) {
  return absl::Substitute("[ts=$0 conn_id=$1 addr=$2]", event.timestamp_ns, ToString(event.conn_id),
                          ::px::ToString(reinterpret_cast<const struct sockaddr*>(&event.addr)));
}

inline std::string ToString(const socket_control_event_t& event) {
  return absl::Substitute("[type=$0 $1]", magic_enum::enum_name(event.type),
                          event.type == kConnOpen ? ToString(event.open) : ToString(event.close));
}

#endif
