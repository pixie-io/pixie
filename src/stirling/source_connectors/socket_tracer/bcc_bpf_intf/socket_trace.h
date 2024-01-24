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

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/socket.h>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"

#define PX_AF_UNKNOWN 0xff

const char kControlMapName[] = "control_map";
const char kControlValuesArrayName[] = "control_values";

const int64_t kTraceAllTGIDs = -1;

// Note: A value of 100 results in >4096 BPF instructions, which is too much for older kernels.
#define CONN_CLEANUP_ITERS 85
const int kMaxConnMapCleanupItems = CONN_CLEANUP_ITERS;

union sockaddr_t {
  struct sockaddr sa;
  struct sockaddr_in in4;
  struct sockaddr_in6 in6;
};

// This struct contains information collected when a connection is established,
// via an accept() syscall.
struct conn_info_t {
  // Connection identifier (PID, FD, etc.).
  struct conn_id_t conn_id;

  // IP address of the local endpoint.
  union sockaddr_t laddr;
  // IP address of the remote endpoint.
  union sockaddr_t raddr;

  // The protocol of traffic on the connection (HTTP, MySQL, etc.).
  enum traffic_protocol_t protocol;

  // Classify traffic as requests, responses or mixed.
  enum endpoint_role_t role;

  // Whether the connection uses SSL.
  bool ssl;

  enum ssl_source_t ssl_source;

  // The number of bytes written/read on this connection.
  int64_t wr_bytes;
  int64_t rd_bytes;

  // The previously reported values of bytes written/read.
  // Used for determining when to send updated conn_stats values.
  int64_t last_reported_bytes;

  // The number of bytes written by application (for uprobe) on this connection.
  int64_t app_wr_bytes;
  // The number of bytes read by application (for uprobe) on this connection.
  int64_t app_rd_bytes;

  // Some stats for protocol inference. Used for threshold-based filtering.
  //
  // How many times the data segments have been classified as the designated protocol.
  int32_t protocol_match_count;
  // How many times traffic inference has been applied on this connection.
  int32_t protocol_total_count;

  // Keep the header of the last packet suspected to be MySQL/Kafka. MySQL/Kafka server does 2
  // separate read syscalls, first to read the header, and second the body of the packet. Thus, we
  // keep a state. (MySQL): Length(3 bytes) + seq_number(1 byte). (Kafka): Length(4 bytes)
  size_t prev_count;
  char prev_buf[4];
  bool prepend_length_header;
};

// This struct is a subset of conn_info_t. It is used to communicate connect/accept events.
// See conn_info_t for descriptions of the members.
struct conn_event_t {
  union sockaddr_t laddr;
  union sockaddr_t raddr;
  enum endpoint_role_t role;
};

// This struct is a subset of conn_info_t. It is used to communicate close events.
// See conn_info_t for descriptions of the members.
struct close_event_t {
  // The number of bytes written and read at time of close.
  int64_t wr_bytes;
  int64_t rd_bytes;
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
#define CHUNK_LIMIT BPF_CHUNK_LIMIT

// Unique ID to all syscalls and a few other notable functions.
// This applies to events sent to user-space.
enum source_function_t {
  kSourceFunctionUnknown,

  // For syscalls.
  kSyscallAccept,
  kSyscallConnect,
  kSyscallClose,
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
  kSyscallSendfile,

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
  struct attr_t {
    // The timestamp when syscall completed (return probe was triggered).
    uint64_t timestamp_ns;

    // Connection identifier (PID, FD, etc.).
    struct conn_id_t conn_id;

    // The protocol of traffic on the connection (HTTP, MySQL, etc.).
    enum traffic_protocol_t protocol;

    // The server-client role.
    enum endpoint_role_t role;

    // The type of the actual data that the msg field encodes, which is used by the caller
    // to determine how to interpret the data.
    enum traffic_direction_t direction;

    // Whether the traffic was collected from an encrypted channel.
    bool ssl;

    enum ssl_source_t ssl_source;

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

    // Whether to prepend length header to the buffer for messages first inferred as Kafka. MySQL
    // may also use this in this future.
    // See infer_kafka_message in protocol_inference.h for details.
    bool prepend_length_header;
    uint32_t length_header;
  } attr;
  char msg[MAX_MSG_SIZE];
};

#define CONN_OPEN (1 << 0)
#define CONN_CLOSE (1 << 1)

struct conn_stats_event_t {
  // The timestamp of the stats event.
  uint64_t timestamp_ns;

  struct conn_id_t conn_id;

  // IP address of the local endpoint.
  union sockaddr_t laddr;
  // IP address of the remote endpoint.
  union sockaddr_t raddr;

  // The server-client role.
  enum endpoint_role_t role;

  // The number of bytes written on this connection.
  int64_t wr_bytes;
  // The number of bytes read on this connection.
  int64_t rd_bytes;

  // Bitmask of flags specifying whether conn open or close have been observed.
  uint32_t conn_events;
};

enum control_event_type_t {
  kConnOpen,
  kConnClose,
};

struct socket_control_event_t {
  enum control_event_type_t type;
  uint64_t timestamp_ns;
  struct conn_id_t conn_id;

  // Represents the syscall or function that produces this event.
  enum source_function_t source_fn;

  union {
    struct conn_event_t open;
    struct close_event_t close;
  };
};

struct connect_args_t {
  const struct sockaddr* addr;
  int32_t fd;
};

struct accept_args_t {
  struct sockaddr* addr;
  struct socket* sock_alloc_socket;
};

struct data_args_t {
  // Represents the function from which this argument group originates.
  enum source_function_t source_fn;

  // Did the data event call sock_sendmsg/sock_recvmsg.
  // Used to filter out read/write and readv/writev calls that are not to sockets.
  bool sock_event;

  int32_t fd;

  // For send()/recv()/write()/read().
  const char* buf;

  // For sendmsg()/recvmsg()/writev()/readv().
  const struct iovec* iov;
  size_t iovlen;

  // For sendmmsg()
  unsigned int* msg_len;

  // For SSL_write_ex and SSL_read_ex
  size_t* ssl_ex_len;
};

struct close_args_t {
  int32_t fd;
};

struct sendfile_args_t {
  int32_t out_fd;
  int32_t in_fd;
  size_t count;
};

static const uint32_t kOpenSSLTraceStatusIdx = 0;

enum openssl_trace_errors_t {
  kOpenSSLTraceOk = 0,
  kOpenSSLMismatchedFDsDetected,
};
