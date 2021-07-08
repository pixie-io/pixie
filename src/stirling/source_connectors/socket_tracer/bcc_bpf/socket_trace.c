/*
 * Copyright 2018- The Pixie Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include <linux/in6.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/inet_sock.h>

#define socklen_t size_t

#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf/utils.h"
#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/protocol_inference.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.h"

// This keeps instruction count below BPF's limit of 4096 per probe.
#define LOOP_LIMIT 45

const int32_t kInvalidFD = -1;

// Determines what percentage of events must be inferred as a certain type for us to consider the
// connection to be of that type. Encoded as a numerator/denominator. Currently set to 20%. While
// this may seem low, one must consider that not all captures are packet-aligned, and the inference
// logic doesn't work on the middle of packets. Moreover, a large data packet would get split up
// and cause issues. This threshold only needs to be larger than the false positive rate, which
// for MySQL is 32/256 based on command only.
const int kTrafficInferenceThresholdNum = 1;
const int kTrafficInferenceThresholdDen = 5;

// This bias is added to the numerator of the traffic inference threshold.
// By using a positive number, it biases messages to be classified as matches,
// when the number of samples is low.
const int kTrafficInferenceBias = 5;

// This is the amount of activity required on a connection before a new ConnStats event
// is reported to user-space. It applies to read and write traffic combined.
const int kConnStatsDataThreshold = 65536;

// This is the perf buffer for BPF program to export data from kernel to user space.
BPF_PERF_OUTPUT(socket_data_events);
BPF_PERF_OUTPUT(socket_control_events);
BPF_PERF_OUTPUT(conn_stats_events);

// This output is used to export notification of processes that have performed an mmap.
BPF_PERF_OUTPUT(mmap_events);

// This control_map is a bit-mask that controls which endpoints are traced in a connection.
// The bits are defined in EndpointRole enum, kRoleClient or kRoleServer. kRoleUnknown is not
// really used, but is defined for completeness.
// There is a control map element for each protocol.
BPF_PERCPU_ARRAY(control_map, uint64_t, kNumProtocols);

// Map from user-space file descriptors to the connections obtained from accept() syscall.
// Tracks connection from accept() -> close().
// Key is {tgid, fd}.
BPF_HASH(conn_info_map, uint64_t, struct conn_info_t, 131072);

// Map to indicate which connections (TGID+FD), user-space has disabled.
// This is tracked separately from conn_info_map to avoid any read-write races.
// This particular map is only written from user-space, and only read from BPF.
// The value is a TSID indicating the last TSID to be disabled. Any newer
// TSIDs should still be pushed out to user space. Events on older TSIDs is not possible.
// Key is {tgid, fd}; Value is TSID.
BPF_HASH(conn_disabled_map, uint64_t, uint64_t);

// Map from user-space file descriptors to open files obtained from open() syscall.
// Used to filter out file read/writes.
// Tracks connection from open() -> close().
// Key is {tgid, fd}.
BPF_HASH(open_file_map, uint64_t, bool);

// Map from thread to its ongoing accept() syscall's input argument.
// Tracks accept() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_accept_args_map, uint64_t, struct accept_args_t);

// Map from thread to its ongoing connect() syscall's input argument.
// Tracks connect() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_connect_args_map, uint64_t, struct connect_args_t);

// Map from thread to its ongoing write() syscall's input argument.
// Tracks write() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_write_args_map, uint64_t, struct data_args_t);

// Map from thread to its ongoing read() syscall's input argument.
// Tracks read() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_read_args_map, uint64_t, struct data_args_t);

// Map from thread to its ongoing close() syscall's input argument.
// Tracks close() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_close_args_map, uint64_t, struct close_args_t);

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(socket_data_event_buffer_heap, struct socket_data_event_t, 1);
BPF_PERCPU_ARRAY(conn_stats_event_buffer_heap, struct conn_stats_event_t, 1);

// This array records singular values that are used by probes. We group them together to reduce the
// number of arrays with only 1 element.
BPF_PERCPU_ARRAY(control_values, int64_t, kNumControlValues);

/***********************************************************
 * General helper functions
 ***********************************************************/

static __inline uint64_t gen_tgid_fd(uint32_t tgid, int fd) {
  return ((uint64_t)tgid << 32) | (uint32_t)fd;
}

static __inline void set_open_file(uint64_t id, int fd) {
  uint32_t tgid = id >> 32;
  uint64_t tgid_fd = gen_tgid_fd(tgid, fd);
  bool kTrue = 1;
  open_file_map.insert(&tgid_fd, &kTrue);
}

static __inline bool is_open_file(uint32_t tgid, int fd) {
  uint64_t tgid_fd = gen_tgid_fd(tgid, fd);
  bool* open_file = open_file_map.lookup(&tgid_fd);
  return (open_file != NULL);
}

static __inline void clear_open_file(uint32_t tgid, int fd) {
  uint64_t tgid_fd = gen_tgid_fd(tgid, fd);
  open_file_map.delete(&tgid_fd);
}

static __inline void init_conn_id(uint32_t tgid, int32_t fd, struct conn_id_t* conn_id) {
  conn_id->upid.tgid = tgid;
  conn_id->upid.start_time_ticks = get_tgid_start_time();
  conn_id->fd = fd;
  conn_id->tsid = bpf_ktime_get_ns();
}

static __inline void init_conn_info(uint32_t tgid, int32_t fd, struct conn_info_t* conn_info) {
  init_conn_id(tgid, fd, &conn_info->conn_id);
  // NOTE: BCC code defaults to 0, because kRoleUnknown is not 0, must explicitly initialize.
  conn_info->role = kRoleUnknown;
  conn_info->addr.sa.sa_family = PX_AF_UNKNOWN;
}

// Be careful calling this function. The automatic creation of BPF map entries can result in a
// BPF map leak if called on unwanted probes.
// How do we make sure we don't leak then? ConnInfoMapManager.ReleaseResources() will clean-up
// the relevant map entries every time a ConnTracker is destroyed.
static __inline struct conn_info_t* get_or_create_conn_info(uint32_t tgid, int32_t fd) {
  uint64_t tgid_fd = gen_tgid_fd(tgid, fd);
  struct conn_info_t new_conn_info = {};
  init_conn_info(tgid, fd, &new_conn_info);
  return conn_info_map.lookup_or_init(&tgid_fd, &new_conn_info);
}

static __inline void set_conn_as_ssl(uint32_t tgid, int32_t fd) {
  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }
  conn_info->ssl = true;
}

static __inline struct socket_data_event_t* fill_socket_data_event(
    enum source_function_t src_fn, enum TrafficDirection direction,
    const struct conn_info_t* conn_info) {
  uint32_t kZero = 0;
  struct socket_data_event_t* event = socket_data_event_buffer_heap.lookup(&kZero);
  if (event == NULL) {
    return NULL;
  }
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.source_fn = src_fn;
  event->attr.ssl = conn_info->ssl;
  event->attr.direction = direction;
  event->attr.conn_id = conn_info->conn_id;
  event->attr.protocol = conn_info->protocol;
  event->attr.role = conn_info->role;
  return event;
}

static __inline struct conn_stats_event_t* fill_conn_stats_event(
    const struct conn_info_t* conn_info) {
  uint32_t kZero = 0;
  struct conn_stats_event_t* event = conn_stats_event_buffer_heap.lookup(&kZero);
  if (event == NULL) {
    return NULL;
  }

  event->conn_id = conn_info->conn_id;
  event->addr = conn_info->addr;
  event->role = conn_info->role;
  event->wr_bytes = conn_info->wr_bytes;
  event->rd_bytes = conn_info->rd_bytes;
  event->conn_events = 0;
  event->timestamp_ns = bpf_ktime_get_ns();
  return event;
}

/***********************************************************
 * Trace filtering functions
 ***********************************************************/

static __inline bool should_trace_sockaddr_family(sa_family_t sa_family) {
  // PX_AF_UNKNOWN means we never traced the accept/connect, and we don't know the sockaddr family.
  // Trace these because they *may* be a sockaddr of interest.
  return sa_family == PX_AF_UNKNOWN || sa_family == AF_INET || sa_family == AF_INET6;
}

// Returns true if detection passes threshold. Right now this is only used for PGSQL.
//
// TODO(yzhao): Remove protocol detection threshold.
static __inline bool protocol_detection_passes_threshold(const struct conn_info_t* conn_info) {
  if (conn_info->protocol == kProtocolPGSQL) {
    // Since some protocols are hard to infer from a single event, we track the inference stats over
    // time, and then use the match rate to determine whether we really want to consider it to be of
    // the protocol or not. This helps reduce polluting events to user-space.
    bool meets_threshold =
        kTrafficInferenceThresholdDen * (conn_info->protocol_match_count + kTrafficInferenceBias) >
        kTrafficInferenceThresholdNum * conn_info->protocol_total_count;
    return meets_threshold;
  }
  return true;
}

// If this returns false, we still will trace summary stats.
static __inline bool should_trace_protocol_data(const struct conn_info_t* conn_info) {
  if (conn_info->protocol == kProtocolUnknown || !protocol_detection_passes_threshold(conn_info)) {
    return false;
  }

  uint32_t protocol = conn_info->protocol;
  uint64_t kZero = 0;
  uint64_t control = *control_map.lookup_or_init(&protocol, &kZero);
  return control & conn_info->role;
}

static __inline bool is_stirling_tgid(const uint32_t tgid) {
  int idx = kStirlingTGIDIndex;
  int64_t* stirling_tgid = control_values.lookup(&idx);
  if (stirling_tgid == NULL) {
    return false;
  }
  return *stirling_tgid == tgid;
}

enum target_tgid_match_result_t {
  TARGET_TGID_UNSPECIFIED,
  TARGET_TGID_ALL,
  TARGET_TGID_MATCHED,
  TARGET_TGID_UNMATCHED,
};

static __inline enum target_tgid_match_result_t match_trace_tgid(const uint32_t tgid) {
  int idx = kTargetTGIDIndex;
  int64_t* target_tgid = control_values.lookup(&idx);
  if (target_tgid == NULL) {
    return TARGET_TGID_UNSPECIFIED;
  }
  if (*target_tgid < 0) {
    // Negative value means trace all.
    return TARGET_TGID_ALL;
  }
  if (*target_tgid == tgid) {
    return TARGET_TGID_MATCHED;
  }
  return TARGET_TGID_UNMATCHED;
}

static __inline void update_traffic_class(struct conn_info_t* conn_info,
                                          enum TrafficDirection direction, const char* buf,
                                          size_t count) {
  if (conn_info == NULL) {
    return;
  }
  conn_info->protocol_total_count += 1;

  // Try to infer connection type (protocol) based on data.
  struct protocol_message_t inferred_protocol = infer_protocol(buf, count, conn_info);

  // Could not infer the traffic.
  if (inferred_protocol.protocol == kProtocolUnknown || conn_info->protocol == kProtocolMongo ||
      conn_info->protocol == kProtocolKafka) {
    return;
  }

  // Update protocol if not set.
  if (conn_info->protocol == kProtocolUnknown) {
    conn_info->protocol = inferred_protocol.protocol;
    conn_info->protocol_match_count = 1;
  } else if (conn_info->protocol == inferred_protocol.protocol) {
    conn_info->protocol_match_count += 1;
  }

  // Update role if not set.
  if (conn_info->role == kRoleUnknown &&
      // As of 2020-01, Redis protocol detection doesn't implement message type detection.
      // There could be more protocols without message type detection in the future.
      inferred_protocol.type != kUnknown) {
    // Classify Role as XOR between direction and req_resp_type:
    //    direction  req_resp_type  => role
    //    ------------------------------------
    //    kEgress    kRequest       => Client
    //    kEgress    KResponse      => Server
    //    kIngress   kRequest       => Server
    //    kIngress   kResponse      => Client
    conn_info->role = ((direction == kEgress) ^ (inferred_protocol.type == kResponse))
                          ? kRoleClient
                          : kRoleServer;
  }
}

/***********************************************************
 * Perf submit functions
 ***********************************************************/

static __inline void read_sockaddr_kernel(struct conn_info_t* conn_info,
                                          const struct socket* socket) {
  // The use of bpf_probe_read_kernel() is required since BCC cannot insert them as expected.
  struct sock* sk = NULL;
  bpf_probe_read_kernel(&sk, sizeof(sk), &socket->sk);

  struct sock_common* sk_common = &sk->__sk_common;
  uint16_t family = -1;
  uint16_t port = -1;

  bpf_probe_read_kernel(&family, sizeof(family), &sk_common->skc_family);
  bpf_probe_read_kernel(&port, sizeof(port), &sk_common->skc_dport);

  if (family == AF_INET) {
    conn_info->addr.in4.sin_family = family;
    conn_info->addr.in4.sin_port = port;

    uint32_t* addr = &conn_info->addr.in4.sin_addr.s_addr;
    bpf_probe_read_kernel(addr, sizeof(*addr), &sk_common->skc_daddr);
  } else if (family == AF_INET6) {
    conn_info->addr.in6.sin6_family = family;
    conn_info->addr.in6.sin6_port = port;

    struct in6_addr* addr = &conn_info->addr.in6.sin6_addr;
    bpf_probe_read_kernel(&addr, sizeof(*addr), &sk_common->skc_v6_daddr);
  }
}

static __inline void submit_new_conn(struct pt_regs* ctx, uint32_t tgid, int32_t fd,
                                     const struct sockaddr* addr, const struct socket* socket,
                                     enum EndpointRole role) {
  struct conn_info_t conn_info = {};
  init_conn_info(tgid, fd, &conn_info);
  if (addr != NULL) {
    conn_info.addr = *((union sockaddr_t*)addr);
  } else if (socket != NULL) {
    read_sockaddr_kernel(&conn_info, socket);
  }
  conn_info.role = role;

  uint64_t tgid_fd = gen_tgid_fd(tgid, fd);
  conn_info_map.update(&tgid_fd, &conn_info);

  // While we keep all sa_family types in conn_info_map,
  // we only send connections with supported protocols to user-space.
  // We use the same filter function to avoid sending data of unwanted connections as well.
  if (!should_trace_sockaddr_family(conn_info.addr.sa.sa_family)) {
    return;
  }

  struct socket_control_event_t control_event = {};
  control_event.type = kConnOpen;
  control_event.timestamp_ns = bpf_ktime_get_ns();
  control_event.conn_id = conn_info.conn_id;
  control_event.open.addr = conn_info.addr;
  control_event.open.role = conn_info.role;

  socket_control_events.perf_submit(ctx, &control_event, sizeof(struct socket_control_event_t));
}

static __inline void submit_close_event(struct pt_regs* ctx, struct conn_info_t* conn_info) {
  struct socket_control_event_t control_event = {};
  control_event.type = kConnClose;
  control_event.timestamp_ns = bpf_ktime_get_ns();
  control_event.conn_id = conn_info->conn_id;
  control_event.close.rd_bytes = conn_info->rd_bytes;
  control_event.close.wr_bytes = conn_info->wr_bytes;

  socket_control_events.perf_submit(ctx, &control_event, sizeof(struct socket_control_event_t));
}

// TODO(yzhao): We can write a test for this, by define a dummy bpf_probe_read() function. Similar
// in idea to a mock in normal C++ code.

// Writes the input buf to event, and submits the event to the corresponding perf buffer.
// Returns the bytes output from the input buf. Note that is not the total bytes submitted to the
// perf buffer, which includes additional metadata.
static __inline void perf_submit_buf(struct pt_regs* ctx, const enum TrafficDirection direction,
                                     const char* buf, size_t buf_size, size_t offset,
                                     struct conn_info_t* conn_info,
                                     struct socket_data_event_t* event) {
  switch (direction) {
    case kEgress:
      event->attr.pos = conn_info->wr_bytes + offset;
      break;
    case kIngress:
      event->attr.pos = conn_info->rd_bytes + offset;
      break;
  }

  // Record original size of packet. This may get truncated below before submit.
  event->attr.msg_size = buf_size;

  // This rest of this function has been written carefully to keep the BPF verifier happy in older
  // kernels, so please take care when modifying.
  //
  // Logically, what we'd like is the following:
  //    size_t msg_size = buf_size < sizeof(event->msg) ? buf_size : sizeof(event->msg);
  //    bpf_probe_read(&event->msg, msg_size, buf);
  //    event->attr.msg_size = buf_size;
  //    socket_data_events.perf_submit(ctx, event, size_to_submit);
  //
  // But this does not work in kernel versions 4.14 or older, for various reasons:
  //  1) the verifier does not like a bpf_probe_read with size 0.
  //       - Useful link: https://www.mail-archive.com/netdev@vger.kernel.org/msg199918.html
  //  2) the verifier does not like a perf_submit that is larger than sizeof(event).
  //
  // While it is often obvious to us humans that these are not problems,
  // the older verifiers can't prove it to themselves.
  //
  // We often try to provide hints to the verifier using approaches like
  // 'if (msg_size > 0)' around the code, but it turns out that clang is often smarter
  // than the verifier, and optimizes away the structural hints we try to provide the verifier.
  //
  // Solution below involves using a volatile asm statement to prevent clang from optimizing away
  // certain code, so that code can reach the BPF verifier, and convince it that everything is
  // safe.
  //
  // Tested to work on the following kernels:
  //   4.14.104
  //   4.15.18 (Ubuntu 4.15.0-96-generic)

  if (buf_size > MAX_MSG_SIZE || buf_size == 0) {
    return;
  }

  // Note that buf_size_minus_1 will be positive due to the if-statement above.
  size_t buf_size_minus_1 = buf_size - 1;

  // Clang is too smart for us, and tries to remove some of the obvious hints we are leaving for the
  // BPF verifier. So we add this NOP volatile statement, so clang can't optimize away some of our
  // if-statements below.
  // By telling clang that buf_size_minus_1 is both an input and output to some black box assembly
  // code, clang has to discard any assumptions on what values this variable can take.
  asm volatile("" : "+r"(buf_size_minus_1) :);

  buf_size = buf_size_minus_1 + 1;

  // This if-statement check is redundant, but is required for the verifier.
  // Buf size is always greater than 0, but the verifier doesn't know that.
  // 4.14 kernels otherwise reject bpf_probe_read with size that they may think is zero.
  if (buf_size_minus_1 < MAX_MSG_SIZE) {
    bpf_probe_read(&event->msg, buf_size, buf);
    event->attr.msg_buf_size = buf_size;
    socket_data_events.perf_submit(ctx, event, sizeof(event->attr) + buf_size);
  }
}

static __inline void perf_submit_wrapper(struct pt_regs* ctx, const enum TrafficDirection direction,
                                         const char* buf, const size_t buf_size,
                                         struct conn_info_t* conn_info,
                                         struct socket_data_event_t* event) {
  int bytes_sent = 0;
  unsigned int i;

#pragma unroll
  for (i = 0; i < CHUNK_LIMIT; ++i) {
    const int bytes_remaining = buf_size - bytes_sent;
    const size_t current_size = bytes_remaining > MAX_MSG_SIZE ? MAX_MSG_SIZE : bytes_remaining;
    perf_submit_buf(ctx, direction, buf + bytes_sent, current_size, bytes_sent, conn_info, event);
    bytes_sent += current_size;
  }
}

static __inline void perf_submit_iovecs(struct pt_regs* ctx, const enum TrafficDirection direction,
                                        const struct iovec* iov, const size_t iovlen,
                                        const size_t total_size, struct conn_info_t* conn_info,
                                        struct socket_data_event_t* event) {
  // NOTE: The loop index 'i' used to be int. BPF verifier somehow conclude that msg_size inside
  // perf_submit_buf(), after a series of assignment, and passed into a function call, can be
  // negative.
  //
  // The issue can be fixed by changing the loop index, or msg_size inside
  // perf_submit_buf(), to unsigned int (changing to size_t does not work either).
  //
  // We prefer changing loop index, as it appears to be the source of triggering BPF verifier's
  // confusion.
  //
  // NOTE: The syscalls for scatter buffers, {send,recv}msg()/{write,read}v(), access buffers in
  // array order. That means they read or fill iov[0], then iov[1], and so on. They return the total
  // size of the written or read data. Therefore, when loop through the buffers, both the number of
  // buffers and the total size need to be checked. More details can be found on their man pages.
  int bytes_sent = 0;
#pragma unroll
  for (unsigned int i = 0; i < LOOP_LIMIT && i < iovlen && bytes_sent < total_size; ++i) {
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &iov[i]);

    const int bytes_remaining = total_size - bytes_sent;
    const size_t iov_size = iov_cpy.iov_len < bytes_remaining ? iov_cpy.iov_len : bytes_remaining;

    // TODO(oazizi/yzhao): Should switch this to go through perf_submit_wrapper.
    //                     We don't have the BPF instruction count to do so right now.
    perf_submit_buf(ctx, direction, iov_cpy.iov_base, iov_size, bytes_sent, conn_info, event);
    bytes_sent += iov_size;
  }
}

/***********************************************************
 * Map cleanup functions
 ***********************************************************/

int conn_cleanup_uprobe(struct pt_regs* ctx) {
  int n = (int)PT_REGS_PARM1(ctx);
  struct conn_id_t* conn_id_list = (struct conn_id_t*)PT_REGS_PARM2(ctx);

#pragma unroll
  for (int i = 0; i < CONN_CLEANUP_ITERS; ++i) {
    struct conn_id_t conn_id = conn_id_list[i];

    // Moving this break above or into the for loop causes us to breach the BPF instruction count.
    // Has not been investigated. Just keep it here for now.
    if (i >= n) {
      break;
    }

    uint64_t tgid_fd = gen_tgid_fd(conn_id.upid.tgid, conn_id.fd);

    // Before deleting, make sure we have the correct generation by checking the TSID.
    // We don't want to accidentally delete a newer generation that has since come into existence.

    struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
    if (conn_info != NULL && conn_info->conn_id.tsid == conn_id.tsid) {
      conn_info_map.delete(&tgid_fd);
    }

    uint64_t* tsid = conn_disabled_map.lookup(&tgid_fd);
    if (tsid != NULL && *tsid == conn_id.tsid) {
      conn_disabled_map.delete(&tgid_fd);
    }
  }

  return 0;
}

/***********************************************************
 * BPF syscall processing functions
 ***********************************************************/

// Table of what events to send to user-space:
//
// SockAddr   | Protocol   ||  Connect/Accept   |   Data      | Close
// -----------|------------||-------------------|-------------|-------
// INET       | Unknown    ||  Yes              |   Summary   | Yes
// INET       | Known      ||  N/A              |   Full      | Yes
// Other      | Unknown    ||  No               |   No        | No
// Other      | Known      ||  N/A              |   No        | No
// Unknown    | Unknown    ||  No*              |   Summary   | Yes
// Unknown    | Known      ||  N/A              |   Full      | Yes
//
// *: Only applicable to accept() syscalls where addr is nullptr. We won't know the remote addr.
//    Since no useful information is traced, just skip it. Will be treated as a case where we
//    missed the accept.

static __inline void process_syscall_open(struct pt_regs* ctx, uint64_t id) {
  int fd = PT_REGS_RC(ctx);

  if (fd < 0) {
    return;
  }

  set_open_file(id, fd);
}

static __inline void process_syscall_connect(struct pt_regs* ctx, uint64_t id,
                                             const struct connect_args_t* args) {
  uint32_t tgid = id >> 32;
  int ret_val = PT_REGS_RC(ctx);

  if (match_trace_tgid(tgid) == TARGET_TGID_UNMATCHED) {
    return;
  }

  if (args->fd < 0) {
    return;
  }

  // We allow EINPROGRESS to go through, which indicates that a NON_BLOCK socket is undergoing
  // handshake.
  //
  // In case connect() eventually fails, any write or read on the fd would fail nonetheless, and we
  // won't see spurious events.
  //
  // In case a separate connect() is called concurrently in another thread, and succeeds
  // immediately, any write or read on the fd would be attributed to the new connection.
  if (ret_val < 0 && ret_val != -EINPROGRESS) {
    return;
  }

  submit_new_conn(ctx, tgid, args->fd, args->addr, /*socket*/ NULL, kRoleClient);
}

static __inline void process_syscall_accept(struct pt_regs* ctx, uint64_t id,
                                            const struct accept_args_t* args) {
  uint32_t tgid = id >> 32;
  int ret_fd = PT_REGS_RC(ctx);

  if (match_trace_tgid(tgid) == TARGET_TGID_UNMATCHED) {
    return;
  }

  if (ret_fd < 0) {
    return;
  }

  submit_new_conn(ctx, tgid, ret_fd, args->addr, args->sock_alloc_socket, kRoleServer);
}

// TODO(oazizi): This is badly broken (but better than before).
//               Suppose a server with a UDP socket has the following sequence:
//                 recvmsg(/*sockfd*/ 5, /*msgaddr*/ A);
//                 recvmsg(/*sockfd*/ 5, /*msgaddr*/ B);
//                 sendmsg(/*sockfd*/ 5, /*msgaddr*/ B);
//                 sendmsg(/*sockfd*/ 5, /*msgaddr*/ A);
//
// This function will produce incorrect results, because it will never register B.
// Everything will be attributed to the first address recorded on the socket.
//
// Note that even if we record address changes, the sequence above will
// not be correct for the last sendmsg in the sequence above.
//
// Problem is our ConnTracker model is not suitable for UDP, where there is no connection.
// For a TCP server, accept() sets the remote address, and all messages on that socket are to/from
// that remote address. For a UDP server, there is no such thing. Every datagram has an address
// specified with it. If we try to record and submit the "connection", then it may not be the right
// remote endpoint for all messages on that socket.
//
// In this example, process_implicit_conn() will get triggered on the first recvmsg, and then
// everything on sockfd=5 will assume to be on that address...which is clearly wrong.
static __inline void process_implicit_conn(struct pt_regs* ctx, uint64_t id,
                                           const struct connect_args_t* args) {
  uint32_t tgid = id >> 32;

  if (match_trace_tgid(tgid) == TARGET_TGID_UNMATCHED) {
    return;
  }

  if (args->fd < 0) {
    return;
  }

  uint64_t tgid_fd = gen_tgid_fd(tgid, args->fd);

  struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
  if (conn_info != NULL) {
    return;
  }

  submit_new_conn(ctx, tgid, args->fd, args->addr, /*socket*/ NULL, kRoleUnknown);
}

static __inline void process_data(const bool vecs, struct pt_regs* ctx, uint64_t id,
                                  const enum TrafficDirection direction,
                                  const struct data_args_t* args, ssize_t bytes_count, bool ssl) {
  uint32_t tgid = id >> 32;

  if (!vecs && args->buf == NULL) {
    return;
  }

  if (vecs && (args->iov == NULL || args->iovlen <= 0)) {
    return;
  }

  if (args->fd < 0) {
    return;
  }

  if (bytes_count <= 0) {
    // This read()/write() call failed, or processed nothing.
    return;
  }

  if (is_open_file(tgid, args->fd)) {
    return;
  }

  enum target_tgid_match_result_t match_result = match_trace_tgid(tgid);
  if (match_result == TARGET_TGID_UNMATCHED) {
    return;
  }

  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, args->fd);
  if (conn_info == NULL) {
    return;
  }

  if (conn_info->ssl && !ssl) {
    // This connection is tracking SSL now.
    // Don't report encrypted data.
    // Also, note this is a special case. We don't delete conn_info_map entry,
    // because we are still tracking the connection.
    return;
  }

  uint64_t tgid_fd = gen_tgid_fd(tgid, args->fd);

  // While we keep all sa_family types in conn_info_map,
  // we only send connections on INET or UNKNOWN to user-space.
  // Why UNKNOWN? Because we may have failed to trace the initial connection.
  // Also, it's very important to send the UNKNOWN cases to user-space,
  // otherwise we may have a BPF map leak from the earlier call to get_or_create_conn_info().
  if (!should_trace_sockaddr_family(conn_info->addr.sa.sa_family)) {
    return;
  }

  // TODO(yzhao): Split the interface such that the singular buf case and multiple bufs in msghdr
  // are handled separately without mixed interface. The plan is to factor out helper functions for
  // lower-level functionalities, and call them separately for each case.
  if (!vecs) {
    update_traffic_class(conn_info, direction, args->buf, bytes_count);
  } else {
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &args->iov[0]);
    // Ensure we are not reading beyond the available data.
    const size_t buf_size = iov_cpy.iov_len < bytes_count ? iov_cpy.iov_len : bytes_count;
    update_traffic_class(conn_info, direction, iov_cpy.iov_base, buf_size);
  }

  uint64_t* conn_disabled_tsid = conn_disabled_map.lookup(&tgid_fd);

  bool send_data = !is_stirling_tgid(tgid) &&
                   (match_result == TARGET_TGID_MATCHED || should_trace_protocol_data(conn_info)) &&
                   (conn_disabled_tsid == NULL || conn_info->conn_id.tsid > *conn_disabled_tsid);

  if (send_data) {
    struct socket_data_event_t* event =
        fill_socket_data_event(args->source_fn, direction, conn_info);
    if (event == NULL) {
      // event == NULL not expected to ever happen.
      return;
    }

    // TODO(yzhao): Same TODO for split the interface.
    if (!vecs) {
      perf_submit_wrapper(ctx, direction, args->buf, bytes_count, conn_info, event);
    } else {
      // TODO(yzhao): iov[0] is copied twice, once in calling update_traffic_class(), and here.
      // This happens to the write probes as well, but the calls are placed in the entry and return
      // probes respectively. Consider remove one copy.
      perf_submit_iovecs(ctx, direction, args->iov, args->iovlen, bytes_count, conn_info, event);
    }
  }

  // Update state of the connection.
  switch (direction) {
    case kEgress:
      conn_info->wr_bytes += bytes_count;
      break;
    case kIngress:
      conn_info->rd_bytes += bytes_count;
      break;
  }

  // Only send event if there's been enough of a change.
  // TODO(oazizi): Add elapsed time since last send as a triggering condition too.
  uint64_t total_bytes = conn_info->wr_bytes + conn_info->rd_bytes;
  bool meets_activity_threshold =
      total_bytes >= conn_info->last_reported_bytes + kConnStatsDataThreshold;
  if (meets_activity_threshold) {
    struct conn_stats_event_t* event = fill_conn_stats_event(conn_info);
    if (event != NULL) {
      conn_stats_events.perf_submit(ctx, event, sizeof(struct conn_stats_event_t));
    }

    conn_info->last_reported_bytes = conn_info->rd_bytes + conn_info->wr_bytes;
  }

  return;
}

// These wrappers around process_data are carefully written so that they call process_data(),
// with a constant for `vecs`. Normally this would be done with meta-programming--for example
// through a template parameter--but C does not support that.
// By using a hard-coded constant for vecs (true or false), we enable clang to clone process_data
// and optimize away some code paths depending on whether we are using the iovecs or buf-based
// version. This is important for reducing the number of BPF instructions, since each syscall only
// needs one particular version.
// TODO(oazizi): Split process_data() into two versions, so we don't have to count on
//               Clang function cloning, which is not directly controllable.

static __inline void process_syscall_data(struct pt_regs* ctx, uint64_t id,
                                          const enum TrafficDirection direction,
                                          const struct data_args_t* args, ssize_t bytes_count) {
  process_data(/* vecs */ false, ctx, id, direction, args, bytes_count, /* ssl */ false);
}

static __inline void process_syscall_data_vecs(struct pt_regs* ctx, uint64_t id,
                                               const enum TrafficDirection direction,
                                               const struct data_args_t* args,
                                               ssize_t bytes_count) {
  process_data(/* vecs */ true, ctx, id, direction, args, bytes_count, /* ssl */ false);
}

static __inline void process_syscall_close(struct pt_regs* ctx, uint64_t id,
                                           const struct close_args_t* close_args) {
  uint32_t tgid = id >> 32;
  int ret_val = PT_REGS_RC(ctx);

  if (close_args->fd < 0) {
    return;
  }

  clear_open_file(tgid, close_args->fd);

  if (ret_val < 0) {
    // This close() call failed.
    return;
  }

  if (match_trace_tgid(tgid) == TARGET_TGID_UNMATCHED) {
    return;
  }

  uint64_t tgid_fd = gen_tgid_fd(tgid, close_args->fd);
  struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
  if (conn_info == NULL) {
    return;
  }

  // Only submit event to user-space if there was a corresponding open or data event reported.
  // This is to avoid polluting the perf buffer.
  if (should_trace_sockaddr_family(conn_info->addr.sa.sa_family) || conn_info->wr_bytes != 0 ||
      conn_info->rd_bytes != 0) {
    submit_close_event(ctx, conn_info);

    // Report final conn stats event for this connection.
    struct conn_stats_event_t* event = fill_conn_stats_event(conn_info);
    if (event != NULL) {
      event->conn_events = event->conn_events | CONN_CLOSE;
      conn_stats_events.perf_submit(ctx, event, sizeof(struct conn_stats_event_t));
    }
  }

  conn_info_map.delete(&tgid_fd);
}

/***********************************************************
 * BPF syscall probe function entry-points
 ***********************************************************/

// The following functions are the tracing function entry points.
// There is an entry probe and a return probe for each syscall.
// Information from both the entry and return probes are required
// before a syscall can be processed.
//
// General structure:
//    Entry probe: responsible for recording arguments.
//    Return probe: responsible for retrieving recorded arguments,
//                  extracting the return value,
//                  and processing the syscall with the combined context.
//
// Syscall signatures are listed. Look for detailed synopses in man pages.

// int open(const char *pathname, int flags);
int syscall__probe_ret_open(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  // No arguments were stashed; non-existent entry probe.
  process_syscall_open(ctx, id);

  return 0;
}

// int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int syscall__probe_entry_connect(struct pt_regs* ctx, int sockfd, const struct sockaddr* addr,
                                 socklen_t addrlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct connect_args_t connect_args = {};
  connect_args.fd = sockfd;
  connect_args.addr = addr;
  active_connect_args_map.update(&id, &connect_args);

  return 0;
}

int syscall__probe_ret_connect(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL) {
    process_syscall_connect(ctx, id, connect_args);
  }

  active_connect_args_map.delete(&id);
  return 0;
}

// int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int syscall__probe_entry_accept(struct pt_regs* ctx, int sockfd, struct sockaddr* addr,
                                socklen_t* addrlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct accept_args_t accept_args = {};
  accept_args.addr = addr;
  active_accept_args_map.update(&id, &accept_args);

  return 0;
}

int syscall__probe_ret_accept(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Unstash arguments, and process syscall.
  struct accept_args_t* accept_args = active_accept_args_map.lookup(&id);
  if (accept_args != NULL) {
    process_syscall_accept(ctx, id, accept_args);
  }

  active_accept_args_map.delete(&id);
  return 0;
}

// int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
int syscall__probe_entry_accept4(struct pt_regs* ctx, int sockfd, struct sockaddr* addr,
                                 socklen_t* addrlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct accept_args_t accept_args = {};
  accept_args.addr = addr;
  active_accept_args_map.update(&id, &accept_args);

  return 0;
}

int syscall__probe_ret_accept4(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Unstash arguments, and process syscall.
  struct accept_args_t* accept_args = active_accept_args_map.lookup(&id);
  if (accept_args != NULL) {
    process_syscall_accept(ctx, id, accept_args);
  }

  active_accept_args_map.delete(&id);
  return 0;
}

// ssize_t write(int fd, const void *buf, size_t count);
int syscall__probe_entry_write(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct data_args_t write_args = {};
  write_args.source_fn = kSyscallWrite;
  write_args.fd = fd;
  write_args.buf = buf;
  active_write_args_map.update(&id, &write_args);

  return 0;
}

int syscall__probe_ret_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  // Don't process FD 0-2 to avoid STDIN, STDOUT, STDERR.
  if (write_args != NULL && write_args->fd > 2) {
    process_syscall_data(ctx, id, kEgress, write_args, bytes_count);
  }

  active_write_args_map.delete(&id);
  return 0;
}

// ssize_t send(int sockfd, const void *buf, size_t len, int flags);
int syscall__probe_entry_send(struct pt_regs* ctx, int sockfd, char* buf, size_t len) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct data_args_t write_args = {};
  write_args.source_fn = kSyscallSend;
  write_args.fd = sockfd;
  write_args.buf = buf;
  active_write_args_map.update(&id, &write_args);

  return 0;
}

int syscall__probe_ret_send(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_syscall_data(ctx, id, kEgress, write_args, bytes_count);
  }

  active_write_args_map.delete(&id);
  return 0;
}

// ssize_t read(int fd, void *buf, size_t count);
int syscall__probe_entry_read(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct data_args_t read_args = {};
  read_args.source_fn = kSyscallRead;
  read_args.fd = fd;
  read_args.buf = buf;
  active_read_args_map.update(&id, &read_args);

  return 0;
}

int syscall__probe_ret_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  // Don't process FD 0-2 to avoid STDIN, STDOUT, STDERR.
  if (read_args != NULL && read_args->fd > 2) {
    process_syscall_data(ctx, id, kIngress, read_args, bytes_count);
  }

  active_read_args_map.delete(&id);
  return 0;
}

// ssize_t recv(int sockfd, void *buf, size_t len, int flags);
int syscall__probe_entry_recv(struct pt_regs* ctx, int sockfd, char* buf, size_t len) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct data_args_t read_args = {};
  read_args.source_fn = kSyscallRecv;
  read_args.fd = sockfd;
  read_args.buf = buf;
  active_read_args_map.update(&id, &read_args);

  return 0;
}

int syscall__probe_ret_recv(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_syscall_data(ctx, id, kIngress, read_args, bytes_count);
  }

  active_read_args_map.delete(&id);
  return 0;
}

// ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
//                const struct sockaddr *dest_addr, socklen_t addrlen);
int syscall__probe_entry_sendto(struct pt_regs* ctx, int sockfd, char* buf, size_t len, int flags,
                                const struct sockaddr* dest_addr, socklen_t addrlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  if (dest_addr != NULL) {
    struct connect_args_t connect_args = {};
    connect_args.fd = sockfd;
    connect_args.addr = dest_addr;
    active_connect_args_map.update(&id, &connect_args);
  }

  // Stash arguments.
  struct data_args_t write_args = {};
  write_args.source_fn = kSyscallSendTo;
  write_args.fd = sockfd;
  write_args.buf = buf;
  active_write_args_map.update(&id, &write_args);

  return 0;
}

int syscall__probe_ret_sendto(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Potential issue: If sentto() addr is provided by a TCP connection, the syscall may ignore it,
  // but we would still trace it. In practice, TCP connections should not be using sendto() with an
  // addr argument.
  //
  // From the man page:
  //   If sendto() is used on a connection-mode (SOCK_STREAM, SOCK_SEQPACKET) socket, the arguments
  //   dest_addr and addrlen are ignored (and the error EISCONN may be returned when they  are not
  //   NULL and 0)
  //
  //   EISCONN
  //   The connection-mode socket was connected already but a recipient was specified. (Now either
  //   this error is returned, or the recipient specification is ignored.)

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL && bytes_count > 0) {
    process_implicit_conn(ctx, id, connect_args);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_syscall_data(ctx, id, kEgress, write_args, bytes_count);
  }

  active_write_args_map.delete(&id);

  return 0;
}

// ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
//                  struct sockaddr *src_addr, socklen_t *addrlen);
int syscall__probe_entry_recvfrom(struct pt_regs* ctx, int sockfd, char* buf, size_t len, int flags,
                                  struct sockaddr* src_addr, socklen_t* addrlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  if (src_addr != NULL) {
    struct connect_args_t connect_args = {};
    connect_args.fd = sockfd;
    connect_args.addr = src_addr;
    active_connect_args_map.update(&id, &connect_args);
  }

  // Stash arguments.
  struct data_args_t read_args = {};
  read_args.source_fn = kSyscallRecvFrom;
  read_args.fd = sockfd;
  read_args.buf = buf;
  active_read_args_map.update(&id, &read_args);

  return 0;
}

int syscall__probe_ret_recvfrom(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL && bytes_count > 0) {
    process_implicit_conn(ctx, id, connect_args);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_syscall_data(ctx, id, kIngress, read_args, bytes_count);
  }
  active_read_args_map.delete(&id);

  return 0;
}

// ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
int syscall__probe_entry_sendmsg(struct pt_regs* ctx, int sockfd,
                                 const struct user_msghdr* msghdr) {
  uint64_t id = bpf_get_current_pid_tgid();

  if (msghdr != NULL) {
    // Stash arguments.
    if (msghdr->msg_name != NULL) {
      struct connect_args_t connect_args = {};
      connect_args.fd = sockfd;
      connect_args.addr = msghdr->msg_name;
      active_connect_args_map.update(&id, &connect_args);
    }

    // Stash arguments.
    struct data_args_t write_args = {};
    write_args.source_fn = kSyscallSendMsg;
    write_args.fd = sockfd;
    write_args.iov = msghdr->msg_iov;
    write_args.iovlen = msghdr->msg_iovlen;
    active_write_args_map.update(&id, &write_args);
  }

  return 0;
}

int syscall__probe_ret_sendmsg(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL && bytes_count > 0) {
    process_implicit_conn(ctx, id, connect_args);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_syscall_data_vecs(ctx, id, kEgress, write_args, bytes_count);
  }

  active_write_args_map.delete(&id);
  return 0;
}

int syscall__probe_entry_sendmmsg(struct pt_regs* ctx, int sockfd, struct mmsghdr* msgvec,
                                  unsigned int vlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // TODO(oazizi): Right now, we only trace the first message in a sendmmsg() call.
  if (msgvec != NULL && vlen >= 1) {
    // Stash arguments.
    if (msgvec[0].msg_hdr.msg_name != NULL) {
      struct connect_args_t connect_args = {};
      connect_args.fd = sockfd;
      connect_args.addr = msgvec[0].msg_hdr.msg_name;
      active_connect_args_map.update(&id, &connect_args);
    }

    // Stash arguments.
    struct data_args_t write_args = {};
    write_args.source_fn = kSyscallSendMMsg;
    write_args.fd = sockfd;
    write_args.iov = msgvec[0].msg_hdr.msg_iov;
    write_args.iovlen = msgvec[0].msg_hdr.msg_iovlen;
    write_args.msg_len = &msgvec[0].msg_len;
    active_write_args_map.update(&id, &write_args);
  }

  return 0;
}

int syscall__probe_ret_sendmmsg(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  int num_msgs = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL && num_msgs > 0) {
    process_implicit_conn(ctx, id, connect_args);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL && num_msgs > 0) {
    ssize_t bytes_count;
    bpf_probe_read(&bytes_count, sizeof(write_args->msg_len), write_args->msg_len);
    process_syscall_data_vecs(ctx, id, kEgress, write_args, bytes_count);
  }
  active_write_args_map.delete(&id);

  return 0;
}

// ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
int syscall__probe_entry_recvmsg(struct pt_regs* ctx, int sockfd, struct user_msghdr* msghdr) {
  uint64_t id = bpf_get_current_pid_tgid();

  if (msghdr != NULL) {
    // Stash arguments.
    if (msghdr->msg_name != NULL) {
      struct connect_args_t connect_args = {};
      connect_args.fd = sockfd;
      connect_args.addr = msghdr->msg_name;
      active_connect_args_map.update(&id, &connect_args);
    }

    // Stash arguments.
    struct data_args_t read_args = {};
    read_args.source_fn = kSyscallRecvMMsg;
    read_args.fd = sockfd;
    read_args.iov = msghdr->msg_iov;
    read_args.iovlen = msghdr->msg_iovlen;
    active_read_args_map.update(&id, &read_args);
  }

  return 0;
}

int syscall__probe_ret_recvmsg(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL && bytes_count > 0) {
    process_implicit_conn(ctx, id, connect_args);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_syscall_data_vecs(ctx, id, kIngress, read_args, bytes_count);
  }

  active_read_args_map.delete(&id);
  return 0;
}

// int recvmmsg(int sockfd, struct mmsghdr *msgvec, unsigned int vlen,
//              int flags, struct timespec *timeout);
int syscall__probe_entry_recvmmsg(struct pt_regs* ctx, int sockfd, struct mmsghdr* msgvec,
                                  unsigned int vlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // TODO(oazizi): Right now, we only trace the first message in a recvmmsg() call.
  if (msgvec != NULL && vlen >= 1) {
    // Stash arguments.
    if (msgvec[0].msg_hdr.msg_name != NULL) {
      struct connect_args_t connect_args = {};
      connect_args.fd = sockfd;
      connect_args.addr = msgvec[0].msg_hdr.msg_name;
      active_connect_args_map.update(&id, &connect_args);
    }

    // Stash arguments.
    struct data_args_t read_args = {};
    read_args.source_fn = kSyscallRecvMMsg;
    read_args.fd = sockfd;
    read_args.iov = msgvec[0].msg_hdr.msg_iov;
    read_args.iovlen = msgvec[0].msg_hdr.msg_iovlen;
    read_args.msg_len = &msgvec[0].msg_len;
    active_read_args_map.update(&id, &read_args);
  }

  return 0;
}

int syscall__probe_ret_recvmmsg(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  int num_msgs = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  const struct connect_args_t* connect_args = active_connect_args_map.lookup(&id);
  if (connect_args != NULL && num_msgs > 0) {
    process_implicit_conn(ctx, id, connect_args);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL && num_msgs > 0) {
    ssize_t bytes_count;
    bpf_probe_read(&bytes_count, sizeof(read_args->msg_len), read_args->msg_len);
    process_syscall_data_vecs(ctx, id, kIngress, read_args, bytes_count);
  }
  active_read_args_map.delete(&id);

  return 0;
}

// ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
int syscall__probe_entry_writev(struct pt_regs* ctx, int fd, const struct iovec* iov, int iovlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct data_args_t write_args = {};
  write_args.source_fn = kSyscallWriteV;
  write_args.fd = fd;
  write_args.iov = iov;
  write_args.iovlen = iovlen;
  active_write_args_map.update(&id, &write_args);

  return 0;
}

int syscall__probe_ret_writev(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_syscall_data_vecs(ctx, id, kEgress, write_args, bytes_count);
  }

  active_write_args_map.delete(&id);
  return 0;
}

// ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
int syscall__probe_entry_readv(struct pt_regs* ctx, int fd, struct iovec* iov, int iovlen) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct data_args_t read_args = {};
  read_args.source_fn = kSyscallReadV;
  read_args.fd = fd;
  read_args.iov = iov;
  read_args.iovlen = iovlen;
  active_read_args_map.update(&id, &read_args);

  return 0;
}

int syscall__probe_ret_readv(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_syscall_data_vecs(ctx, id, kIngress, read_args, bytes_count);
  }

  active_read_args_map.delete(&id);
  return 0;
}

// int close(int fd);
int syscall__probe_entry_close(struct pt_regs* ctx, unsigned int fd) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct close_args_t close_args;
  close_args.fd = fd;
  active_close_args_map.update(&id, &close_args);

  return 0;
}

int syscall__probe_ret_close(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Unstash arguments, and process syscall.
  const struct close_args_t* close_args = active_close_args_map.lookup(&id);
  if (close_args != NULL) {
    process_syscall_close(ctx, id, close_args);
  }

  active_close_args_map.delete(&id);
  return 0;
}

// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
int syscall__probe_entry_mmap(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  struct upid_t upid = {};
  upid.tgid = id >> 32;
  upid.start_time_ticks = get_tgid_start_time();

  mmap_events.perf_submit(ctx, &upid, sizeof(upid));

  return 0;
}

// Trace kernel function:
// struct socket *sock_alloc(void)
// which is called inside accept4() syscall to allocate socket data structure.
// Only need a return probe, as the function does not accept any arguments.
int probe_ret_sock_alloc(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Only trace sock_alloc() called by accept()/accept4().
  struct accept_args_t* accept_args = active_accept_args_map.lookup(&id);
  if (accept_args == NULL) {
    return 0;
  }

  if (accept_args->sock_alloc_socket == NULL) {
    accept_args->sock_alloc_socket = (struct socket*)PT_REGS_RC(ctx);
  }

  return 0;
}

// TODO(oazizi): Look into the following opens:
// 1) Why does the syscall table only include sendto, while Linux source code and man page list both
// sendto and send?

// Include HTTP2 tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/go_http2_trace.c"

// Include OpenSSL tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/openssl_trace.c"

// Include GoTLS tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/go_tls_trace.c"
