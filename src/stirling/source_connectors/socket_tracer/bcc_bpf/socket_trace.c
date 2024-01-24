/*
 * This code runs using bpf in the Linux kernel.
 * Copyright 2018- The Pixie Authors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include <linux/in6.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/inet_sock.h>

#define socklen_t size_t

#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf/utils.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/protocol_inference.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.h"
#include "src/stirling/upid/upid.h"

// This keeps instruction count below BPF's limit of 4096 per probe.
#define LOOP_LIMIT BPF_LOOP_LIMIT
#define PROTOCOL_VEC_LIMIT 3

const int32_t kInvalidFD = -1;

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
// The bits are defined in endpoint_role_t enum, kRoleClient or kRoleServer. kRoleUnknown is not
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

struct nested_syscall_fd_t {
  int fd;
  bool mismatched_fds;
};

// Map used to verify if SSL_write or SSL_read are on the stack during a syscall in
// order to propagate the socket fd back to the SSL_write and SSL_read return probes.
// Key is pid tgid.
// Value is nested_syscall_fd struct.
BPF_HASH(ssl_user_space_call_map, uint64_t, struct nested_syscall_fd_t);

BPF_HASH(openssl_source_map, uint32_t, enum ssl_source_t);

// Map from thread to its ongoing close() syscall's input argument.
// Tracks close() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_close_args_map, uint64_t, struct close_args_t);

// Map from thread to its ongoing sendfile syscall's input argument.
// Tracks sendfile() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_sendfile_args_map, uint64_t, struct sendfile_args_t);

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
  conn_info->laddr.sa.sa_family = PX_AF_UNKNOWN;
  conn_info->raddr.sa.sa_family = PX_AF_UNKNOWN;
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

static __inline enum ssl_source_t get_ssl_source(uint32_t tgid) {
  const enum ssl_source_t* ssl_source = openssl_source_map.lookup(&tgid);
  if (ssl_source == NULL) {
    return kSSLUnspecified;
  }
  return *ssl_source;
}

static __inline void set_conn_as_ssl(uint32_t tgid, int32_t fd, enum ssl_source_t ssl_source) {
  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }
  conn_info->ssl = true;
  conn_info->ssl_source = ssl_source;
}

// Writes the syscall fd to a BPF map key created by an active tls call (the SSL_write/SSL_read
// probes and ensures that set_conn_as_ssl is called for the tgid and pid. The majority of syscall
// probes should call this on function entry, however, there are certain probes which are
// not exlusively used for networking (read/write/readev/writev) and therefore must defer
// this fd propagation to its ret probe (where it can be validated it is socket related
// first via sock_event).
static __inline void propagate_fd_to_user_space_call(uint64_t pid_tgid, int fd) {
  struct nested_syscall_fd_t* nested_syscall_fd_ptr = ssl_user_space_call_map.lookup(&pid_tgid);
  if (nested_syscall_fd_ptr != NULL) {
    int current_fd = nested_syscall_fd_ptr->fd;
    if (current_fd == kInvalidFD) {
      nested_syscall_fd_ptr->fd = fd;
    } else if (current_fd != fd) {
      // Found two different fds during a single SSL_write/SSL_read call. This invalidates
      // our tls tracing assumptions and must be recorded.
      nested_syscall_fd_ptr->mismatched_fds = true;
    }

    uint32_t tgid = pid_tgid >> 32;
    set_conn_as_ssl(tgid, fd, get_ssl_source(tgid));
  }
}

static __inline struct socket_data_event_t* fill_socket_data_event(
    enum source_function_t src_fn, enum traffic_direction_t direction,
    const struct conn_info_t* conn_info) {
  uint32_t kZero = 0;
  struct socket_data_event_t* event = socket_data_event_buffer_heap.lookup(&kZero);
  if (event == NULL) {
    return NULL;
  }
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.source_fn = src_fn;
  event->attr.ssl = conn_info->ssl;
  event->attr.ssl_source = conn_info->ssl_source;
  event->attr.direction = direction;
  event->attr.conn_id = conn_info->conn_id;
  event->attr.protocol = conn_info->protocol;
  event->attr.role = conn_info->role;
  event->attr.pos = (direction == kEgress) ? conn_info->wr_bytes : conn_info->rd_bytes;
  event->attr.prepend_length_header = conn_info->prepend_length_header;
  BPF_PROBE_READ_VAR(event->attr.length_header, conn_info->prev_buf);
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
  event->laddr = conn_info->laddr;
  event->raddr = conn_info->raddr;
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

static __inline bool should_trace_conn(struct conn_info_t* conn_info) {
  // While we keep all sa_family types in conn_info_map,
  // we only send connections on INET or UNKNOWN to user-space.
  // Also, it's very important to send the UNKNOWN cases to user-space,
  // otherwise we may have a BPF map leak from the earlier call to get_or_create_conn_info().
  return should_trace_sockaddr_family(conn_info->raddr.sa.sa_family);
}

// If this returns false, we still will trace summary stats.
static __inline bool should_trace_protocol_data(const struct conn_info_t* conn_info) {
  if (conn_info->protocol == kProtocolUnknown) {
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
  // TODO(yzhao): Use externally-defined macro to replace BPF_MAP. Since this function is called for
  // all PIDs, this optimization is useful.
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
                                          enum traffic_direction_t direction, const char* buf,
                                          size_t count) {
  if (conn_info == NULL) {
    return;
  }
  conn_info->protocol_total_count += 1;

  // Try to infer connection type (protocol) based on data.
  struct protocol_message_t inferred_protocol = infer_protocol(buf, count, conn_info);

  // Could not infer the traffic.
  if (inferred_protocol.protocol == kProtocolUnknown) {
    return;
  }

  // Update protocol if not set.
  if (conn_info->protocol == kProtocolUnknown) {
    conn_info->protocol = inferred_protocol.protocol;
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
  // Use BPF_PROBE_READ_KERNEL_VAR since BCC cannot insert them as expected.
  struct sock* sk = NULL;
  BPF_PROBE_READ_KERNEL_VAR(sk, &socket->sk);

  struct sock_common* sk_common = &sk->__sk_common;
  uint16_t family = -1;
  uint16_t lport = -1;
  uint16_t rport = -1;

  BPF_PROBE_READ_KERNEL_VAR(family, &sk_common->skc_family);
  BPF_PROBE_READ_KERNEL_VAR(lport, &sk_common->skc_num);
  BPF_PROBE_READ_KERNEL_VAR(rport, &sk_common->skc_dport);

  conn_info->laddr.sa.sa_family = family;
  conn_info->raddr.sa.sa_family = family;

  if (family == AF_INET) {
    conn_info->laddr.in4.sin_port = lport;
    conn_info->raddr.in4.sin_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(conn_info->laddr.in4.sin_addr.s_addr, &sk_common->skc_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(conn_info->raddr.in4.sin_addr.s_addr, &sk_common->skc_daddr);
  } else if (family == AF_INET6) {
    conn_info->laddr.in6.sin6_port = lport;
    conn_info->raddr.in6.sin6_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(conn_info->laddr.in6.sin6_addr, &sk_common->skc_v6_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(conn_info->raddr.in6.sin6_addr, &sk_common->skc_v6_daddr);
  }
}

static __inline void submit_new_conn(struct pt_regs* ctx, uint32_t tgid, int32_t fd,
                                     const struct sockaddr* addr, const struct socket* socket,
                                     enum endpoint_role_t role, enum source_function_t source_fn) {
  struct conn_info_t conn_info = {};
  init_conn_info(tgid, fd, &conn_info);
  if (socket != NULL) {
    read_sockaddr_kernel(&conn_info, socket);
  } else if (addr != NULL) {
    conn_info.raddr = *((union sockaddr_t*)addr);
  }
  conn_info.role = role;

  uint64_t tgid_fd = gen_tgid_fd(tgid, fd);
  conn_info_map.update(&tgid_fd, &conn_info);

  // While we keep all sa_family types in conn_info_map,
  // we only send connections with supported protocols to user-space.
  // We use the same filter function to avoid sending data of unwanted connections as well.
  if (!should_trace_sockaddr_family(conn_info.raddr.sa.sa_family)) {
    return;
  }

  struct socket_control_event_t control_event = {};
  control_event.type = kConnOpen;
  control_event.timestamp_ns = bpf_ktime_get_ns();
  control_event.conn_id = conn_info.conn_id;
  control_event.source_fn = source_fn;
  control_event.open.raddr = conn_info.raddr;
  control_event.open.laddr = conn_info.laddr;
  control_event.open.role = conn_info.role;

  socket_control_events.perf_submit(ctx, &control_event, sizeof(struct socket_control_event_t));
}

static __inline void submit_close_event(struct pt_regs* ctx, struct conn_info_t* conn_info,
                                        enum source_function_t source_fn) {
  struct socket_control_event_t control_event = {};
  control_event.type = kConnClose;
  control_event.timestamp_ns = bpf_ktime_get_ns();
  control_event.conn_id = conn_info->conn_id;
  control_event.source_fn = source_fn;
  control_event.close.rd_bytes = conn_info->rd_bytes;
  control_event.close.wr_bytes = conn_info->wr_bytes;

  socket_control_events.perf_submit(ctx, &control_event, sizeof(struct socket_control_event_t));
}

// Writes the input buf to event, and submits the event to the corresponding perf buffer.
// Returns the bytes output from the input buf. Note that is not the total bytes submitted to the
// perf buffer, which includes additional metadata.
static __inline void perf_submit_buf(struct pt_regs* ctx, const enum traffic_direction_t direction,
                                     const char* buf, size_t buf_size,
                                     struct conn_info_t* conn_info,
                                     struct socket_data_event_t* event) {
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
  //   4.14

  if (buf_size == 0) {
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

  // 4.14 kernels reject bpf_probe_read with size that they may think is zero.
  // Without the if statement, it somehow can't reason that the bpf_probe_read is non-zero.
  size_t amount_copied = 0;
  if (buf_size_minus_1 < MAX_MSG_SIZE) {
    bpf_probe_read(&event->msg, buf_size, buf);
    amount_copied = buf_size;
  } else if (buf_size_minus_1 < 0x7fffffff) {
    // If-statement condition above is only required to prevent clang from optimizing
    // away the `if (amount_copied > 0)` below.
    bpf_probe_read(&event->msg, MAX_MSG_SIZE, buf);
    amount_copied = MAX_MSG_SIZE;
  }

  // If-statement is redundant, but is required to keep the 4.14 verifier happy.
  if (amount_copied > 0) {
    event->attr.msg_buf_size = amount_copied;
    socket_data_events.perf_submit(ctx, event, sizeof(event->attr) + amount_copied);
  }
}

static __inline void perf_submit_wrapper(struct pt_regs* ctx,
                                         const enum traffic_direction_t direction, const char* buf,
                                         const size_t buf_size, struct conn_info_t* conn_info,
                                         struct socket_data_event_t* event) {
  int bytes_sent = 0;
  unsigned int i;

#pragma unroll
  for (i = 0; i < CHUNK_LIMIT; ++i) {
    const int bytes_remaining = buf_size - bytes_sent;
    const size_t current_size =
        (bytes_remaining > MAX_MSG_SIZE && (i != CHUNK_LIMIT - 1)) ? MAX_MSG_SIZE : bytes_remaining;
    perf_submit_buf(ctx, direction, buf + bytes_sent, current_size, conn_info, event);
    bytes_sent += current_size;

    // Move the position for the next event.
    event->attr.pos += current_size;
  }
}

static __inline void perf_submit_iovecs(struct pt_regs* ctx,
                                        const enum traffic_direction_t direction,
                                        const struct iovec* iov, const size_t iovlen,
                                        const size_t total_size, struct conn_info_t* conn_info,
                                        struct socket_data_event_t* event) {
  // NOTE: The syscalls for scatter buffers, {send,recv}msg()/{write,read}v(), access buffers in
  // array order. That means they read or fill iov[0], then iov[1], and so on. They return the total
  // size of the written or read data. Therefore, when loop through the buffers, both the number of
  // buffers and the total size need to be checked. More details can be found on their man pages.
  int bytes_sent = 0;
#pragma unroll
  for (int i = 0; i < LOOP_LIMIT && i < iovlen && bytes_sent < total_size; ++i) {
    struct iovec iov_cpy;
    BPF_PROBE_READ_VAR(iov_cpy, &iov[i]);

    const int bytes_remaining = total_size - bytes_sent;
    const size_t iov_size = min_size_t(iov_cpy.iov_len, bytes_remaining);

    // TODO(oazizi/yzhao): Should switch this to go through perf_submit_wrapper.
    //                     We don't have the BPF instruction count to do so right now.
    perf_submit_buf(ctx, direction, iov_cpy.iov_base, iov_size, conn_info, event);
    bytes_sent += iov_size;

    // Move the position for the next event.
    event->attr.pos += iov_size;
  }

  // TODO(oazizi): If there is data left after the loop limit, we should still report the remainder
  //               with a data-less event.
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

  submit_new_conn(ctx, tgid, args->fd, args->addr, /*socket*/ NULL, kRoleClient, kSyscallConnect);
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

  submit_new_conn(ctx, tgid, ret_fd, args->addr, args->sock_alloc_socket, kRoleServer,
                  kSyscallAccept);
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
                                           const struct connect_args_t* args,
                                           enum source_function_t source_fn) {
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

  submit_new_conn(ctx, tgid, args->fd, args->addr, /*socket*/ NULL, kRoleUnknown, source_fn);
}

static __inline bool should_send_data(uint32_t tgid, uint64_t conn_disabled_tsid,
                                      bool force_trace_tgid, struct conn_info_t* conn_info) {
  // Never trace stirling.
  if (is_stirling_tgid(tgid)) {
    return false;
  }

  // Never trace any connections that user-space has asked us to disable.
  if (conn_info->conn_id.tsid <= conn_disabled_tsid) {
    return false;
  }

  // Only trace data for protocols of interest, or if forced on.
  return (force_trace_tgid || should_trace_protocol_data(conn_info));
}

static __inline void update_conn_stats(struct pt_regs* ctx, struct conn_info_t* conn_info,
                                       enum traffic_direction_t direction, ssize_t bytes_count) {
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
}

static __inline void process_data(const bool vecs, struct pt_regs* ctx, uint64_t id,
                                  const enum traffic_direction_t direction,
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

  enum target_tgid_match_result_t match_result = match_trace_tgid(tgid);
  if (match_result == TARGET_TGID_UNMATCHED) {
    return;
  }
  bool force_trace_tgid = (match_result == TARGET_TGID_MATCHED);

  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, args->fd);
  if (conn_info == NULL) {
    return;
  }

  if (!should_trace_conn(conn_info)) {
    return;
  }

  uint64_t tgid_fd = gen_tgid_fd(tgid, args->fd);
  uint64_t* conn_disabled_tsid_ptr = conn_disabled_map.lookup(&tgid_fd);
  uint64_t conn_disabled_tsid = (conn_disabled_tsid_ptr == NULL) ? 0 : *conn_disabled_tsid_ptr;

  // Only process plaintext data.
  if (conn_info->ssl == ssl) {
    // TODO(yzhao): Split the interface such that the singular buf case and multiple bufs in msghdr
    // are handled separately without mixed interface. The plan is to factor out helper functions
    // for lower-level functionalities, and call them separately for each case.
    if (!vecs) {
      update_traffic_class(conn_info, direction, args->buf, bytes_count);
    } else {
      struct iovec iov_cpy;
      size_t buf_size = 0;
      // With vectorized buffers, there can be empty elements sent.
      // For protocol inference, it requires a non empty buffer to get the real data

#pragma unroll
      for (size_t i = 0; i < PROTOCOL_VEC_LIMIT && i < args->iovlen; i++) {
        BPF_PROBE_READ_VAR(iov_cpy, &args->iov[i]);
        buf_size = min_size_t(iov_cpy.iov_len, bytes_count);
        if (buf_size != 0) {
          update_traffic_class(conn_info, direction, iov_cpy.iov_base, buf_size);
          break;
        }
      }
    }

    if (should_send_data(tgid, conn_disabled_tsid, force_trace_tgid, conn_info)) {
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
        // This happens to the write probes as well, but the calls are placed in the entry and
        // return probes respectively. Consider remove one copy.
        perf_submit_iovecs(ctx, direction, args->iov, args->iovlen, bytes_count, conn_info, event);
      }
    }
  }

  // TODO(oazizi): For conn stats, we should be using the encrypted traffic to do the accounting,
  //               but that will break things with how we track data positions.
  //               For now, keep using plaintext data. In the future, this if statement should be:
  //                     if (!ssl) { ... }
  if (conn_info->ssl == ssl) {
    update_conn_stats(ctx, conn_info, direction, bytes_count);
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
                                          const enum traffic_direction_t direction,
                                          const struct data_args_t* args, ssize_t bytes_count) {
  process_data(/* vecs */ false, ctx, id, direction, args, bytes_count, /* ssl */ false);
}

static __inline void process_syscall_data_vecs(struct pt_regs* ctx, uint64_t id,
                                               const enum traffic_direction_t direction,
                                               const struct data_args_t* args,
                                               ssize_t bytes_count) {
  process_data(/* vecs */ true, ctx, id, direction, args, bytes_count, /* ssl */ false);
}

static __inline void process_syscall_sendfile(struct pt_regs* ctx, uint64_t id,
                                              const struct sendfile_args_t* args,
                                              ssize_t bytes_count) {
  uint32_t tgid = id >> 32;

  if (args->out_fd < 0) {
    return;
  }

  if (bytes_count <= 0) {
    // This sendfile call failed, or processed nothing.
    return;
  }

  enum target_tgid_match_result_t match_result = match_trace_tgid(tgid);
  if (match_result == TARGET_TGID_UNMATCHED) {
    return;
  }
  bool force_trace_tgid = (match_result == TARGET_TGID_MATCHED);

  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, args->out_fd);
  if (conn_info == NULL) {
    return;
  }

  if (!should_trace_conn(conn_info)) {
    return;
  }

  uint64_t tgid_fd = gen_tgid_fd(tgid, args->out_fd);
  uint64_t* conn_disabled_tsid_ptr = conn_disabled_map.lookup(&tgid_fd);
  uint64_t conn_disabled_tsid = (conn_disabled_tsid_ptr == NULL) ? 0 : *conn_disabled_tsid_ptr;

  if (should_send_data(tgid, conn_disabled_tsid, force_trace_tgid, conn_info)) {
    struct socket_data_event_t* event =
        fill_socket_data_event(kSyscallSendfile, kEgress, conn_info);
    if (event == NULL) {
      // event == NULL not expected to ever happen.
      return;
    }

    event->attr.pos = conn_info->wr_bytes;
    event->attr.msg_size = bytes_count;
    event->attr.msg_buf_size = 0;
    socket_data_events.perf_submit(ctx, event, sizeof(event->attr));
  }

  update_conn_stats(ctx, conn_info, kEgress, bytes_count);

  return;
}

static __inline void process_syscall_close(struct pt_regs* ctx, uint64_t id,
                                           const struct close_args_t* close_args) {
  uint32_t tgid = id >> 32;
  int ret_val = PT_REGS_RC(ctx);

  if (close_args->fd < 0) {
    return;
  }

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
  if (should_trace_sockaddr_family(conn_info->raddr.sa.sa_family) || conn_info->wr_bytes != 0 ||
      conn_info->rd_bytes != 0) {
    submit_close_event(ctx, conn_info, kSyscallClose);

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
  if (write_args != NULL && write_args->sock_event) {
    // Syscalls that aren't exclusively used for networking must be
    // validated to be a sock_event before propagating a socket fd to the
    // tls tracing probes
    propagate_fd_to_user_space_call(id, write_args->fd);
    process_syscall_data(ctx, id, kEgress, write_args, bytes_count);
  }

  active_write_args_map.delete(&id);
  return 0;
}

// ssize_t send(int sockfd, const void *buf, size_t len, int flags);
int syscall__probe_entry_send(struct pt_regs* ctx, int sockfd, char* buf, size_t len) {
  uint64_t id = bpf_get_current_pid_tgid();

  propagate_fd_to_user_space_call(id, sockfd);

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
  if (read_args != NULL && read_args->sock_event) {
    // Syscalls that aren't exclusively used for networking must be
    // validated to be a sock_event before propagating a socket fd to the
    // tls tracing probes
    propagate_fd_to_user_space_call(id, read_args->fd);
    process_syscall_data(ctx, id, kIngress, read_args, bytes_count);
  }

  active_read_args_map.delete(&id);
  return 0;
}

// ssize_t recv(int sockfd, void *buf, size_t len, int flags);
int syscall__probe_entry_recv(struct pt_regs* ctx, int sockfd, char* buf, size_t len) {
  uint64_t id = bpf_get_current_pid_tgid();

  propagate_fd_to_user_space_call(id, sockfd);

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

  propagate_fd_to_user_space_call(id, sockfd);

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
    process_implicit_conn(ctx, id, connect_args, kSyscallSendTo);
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

  propagate_fd_to_user_space_call(id, sockfd);

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
    process_implicit_conn(ctx, id, connect_args, kSyscallRecvFrom);
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

  propagate_fd_to_user_space_call(id, sockfd);

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
    process_implicit_conn(ctx, id, connect_args, kSyscallSendMsg);
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

  propagate_fd_to_user_space_call(id, sockfd);

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
    process_implicit_conn(ctx, id, connect_args, kSyscallSendMMsg);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL && num_msgs > 0) {
    // msg_len is defined as unsigned int, so we have to use the same here.
    // This is different than most other syscalls that use ssize_t.
    unsigned int bytes_count = 0;
    BPF_PROBE_READ_VAR(bytes_count, write_args->msg_len);
    process_syscall_data_vecs(ctx, id, kEgress, write_args, bytes_count);
  }
  active_write_args_map.delete(&id);

  return 0;
}

// ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
int syscall__probe_entry_recvmsg(struct pt_regs* ctx, int sockfd, struct user_msghdr* msghdr) {
  uint64_t id = bpf_get_current_pid_tgid();

  propagate_fd_to_user_space_call(id, sockfd);

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
    read_args.source_fn = kSyscallRecvMsg;
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
    process_implicit_conn(ctx, id, connect_args, kSyscallRecvMsg);
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

  propagate_fd_to_user_space_call(id, sockfd);

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
    process_implicit_conn(ctx, id, connect_args, kSyscallRecvMMsg);
  }
  active_connect_args_map.delete(&id);

  // Unstash arguments, and process syscall.
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL && num_msgs > 0) {
    // msg_len is defined as unsigned int, so we have to use the same here.
    // This is different than most other syscalls that use ssize_t.
    unsigned int bytes_count = 0;
    BPF_PROBE_READ_VAR(bytes_count, read_args->msg_len);
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
  if (write_args != NULL && write_args->sock_event) {
    // Syscalls that aren't exclusively used for networking must be
    // validated to be a sock_event before propagating a socket fd to the
    // tls tracing probes
    propagate_fd_to_user_space_call(id, write_args->fd);
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
  if (read_args != NULL && read_args->sock_event) {
    // Syscalls that aren't exclusively used for networking must be
    // validated to be a sock_event before propagating a socket fd to the
    // tls tracing probes
    propagate_fd_to_user_space_call(id, read_args->fd);
    process_syscall_data_vecs(ctx, id, kIngress, read_args, bytes_count);
  }

  active_read_args_map.delete(&id);
  return 0;
}

// int close(int fd);
int syscall__probe_entry_close(struct pt_regs* ctx, int fd) {
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

// int close(int fd);
int syscall__probe_entry_sendfile(struct pt_regs* ctx, int out_fd, int in_fd, off_t* offset,
                                  size_t count) {
  uint64_t id = bpf_get_current_pid_tgid();

  // Stash arguments.
  struct sendfile_args_t args;
  args.out_fd = out_fd;
  args.in_fd = in_fd;
  args.count = count;
  active_sendfile_args_map.update(&id, &args);

  return 0;
}

int syscall__probe_ret_sendfile(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t bytes_count = PT_REGS_RC(ctx);

  // Unstash arguments, and process syscall.
  const struct sendfile_args_t* args = active_sendfile_args_map.lookup(&id);
  if (args != NULL) {
    process_syscall_sendfile(ctx, id, args, bytes_count);
  }

  active_sendfile_args_map.delete(&id);
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

// Trace kernel function:
// int security_socket_sendmsg(struct socket *sock, struct msghdr *msg, int size)
// which is called by write/writev/send/sendmsg.
int probe_entry_security_socket_sendmsg(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  struct data_args_t* write_args = active_write_args_map.lookup(&id);
  if (write_args != NULL) {
    write_args->sock_event = true;
  }
  return 0;
}

// Trace kernel function:
// int security_socket_recvmsg(struct socket *sock, struct msghdr *msg, int size)
int probe_entry_security_socket_recvmsg(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  struct data_args_t* read_args = active_read_args_map.lookup(&id);
  if (read_args != NULL) {
    read_args->sock_event = true;
  }
  return 0;
}

// OpenSSL tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/openssl_trace.c"

// Go HTTP2 tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/go_http2_trace.c"

// GoTLS tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/go_tls_trace.c"

// gRPC-c tracing probes.
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/grpc_c_trace.c"
