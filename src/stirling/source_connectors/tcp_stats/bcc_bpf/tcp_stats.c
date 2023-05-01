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

#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf/utils.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"
#include "src/stirling/upid/upid.h"

// Map to store TCP socket information.
// Key is {tid}.
// Value is TCP socket.
// size is the default map size 10240.
// At given point of time, expecting that ~5000 TCP connections will be active.
// Also, The code ensures that map is cleaned up as soon as the data collected,
// so default hash map size 10240 serves without conflicts.
BPF_HASH(sock_store, uint32_t, struct sock*, 10240);

// This is the perf buffer for BPF program to export TCP data from kernel to user space.
BPF_PERF_OUTPUT(tcp_events);

static bool valid_ip_family(int family) {
  if (family == AF_INET || family == AF_INET6) {
    return true;
  }
  return false;
}

static int tcp_sendstat(struct pt_regs* ctx, uint32_t tgid, uint32_t id, int size) {
  struct sock** sockpp;
  sockpp = sock_store.lookup(&id);
  if (sockpp == 0) {
    return 0;  // entry missed
  }

  struct sock* sk = *sockpp;
  struct sock_common* sk_common = &sk->__sk_common;
  uint16_t family = -1;

  BPF_PROBE_READ_KERNEL_VAR(family, &sk_common->skc_family);
  if (!valid_ip_family(family)) {
    return 0;
  }

  int lport = -1;
  int rport = -1;
  BPF_PROBE_READ_KERNEL_VAR(rport, &sk_common->skc_dport);
  BPF_PROBE_READ_KERNEL_VAR(lport, &sk_common->skc_num);

  struct tcp_event_t event = {};
  event.remote_addr.sa.sa_family = family;
  event.local_addr.sa.sa_family = family;
  event.timestamp_ns = bpf_ktime_get_ns();
  event.upid.tgid = tgid;
  event.upid.start_time_ticks = get_tgid_start_time();

  if (family == AF_INET) {
    event.local_addr.in4.sin_port = ntohs(lport);
    event.remote_addr.in4.sin_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(event.local_addr.in4.sin_addr.s_addr, &sk_common->skc_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(event.remote_addr.in4.sin_addr.s_addr, &sk_common->skc_daddr);
  } else if (family == AF_INET6) {
    event.local_addr.in6.sin6_port = ntohs(lport);
    event.remote_addr.in6.sin6_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(event.local_addr.in6.sin6_addr, &sk_common->skc_v6_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(event.remote_addr.in6.sin6_addr, &sk_common->skc_v6_daddr);
  }

  event.size = size;
  event.type = kTCPTx;
  tcp_events.perf_submit(ctx, &event, sizeof(event));

  sock_store.delete(&id);
  return 0;
}

int probe_ret_tcp_sendmsg(struct pt_regs* ctx) {
  int size = PT_REGS_RC(ctx);
  if (size > 0) {
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t tgid = id >> 32;
    return tcp_sendstat(ctx, tgid, id, size);
  } else {
    return 0;
  }
}

int probe_ret_tcp_sendpage(struct pt_regs* ctx) {
  int size = PT_REGS_RC(ctx);
  if (size > 0) {
    uint64_t id = bpf_get_current_pid_tgid();
    uint32_t tgid = id >> 32;
    return tcp_sendstat(ctx, tgid, id, size);
  } else {
    return 0;
  }
}

static int tcp_send_entry(struct sock* sk) {
  uint32_t tid = bpf_get_current_pid_tgid();
  sock_store.update(&tid, &sk);
  return 0;
}

int probe_entry_tcp_sendpage(struct pt_regs* ctx, struct sock* sk, struct page* page, int offset,
                             size_t size) {
  return tcp_send_entry(sk);
}

int probe_entry_tcp_sendmsg(struct pt_regs* ctx, struct sock* sk, struct msghdr* msg, size_t size) {
  return tcp_send_entry(sk);
}

int probe_entry_tcp_cleanup_rbuf(struct pt_regs* ctx, struct sock* sk, int copied) {
  if (copied <= 0) {
    return 0;
  }
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;

  uint16_t family = -1;

  BPF_PROBE_READ_KERNEL_VAR(family, &sk->__sk_common.skc_family);
  if (!valid_ip_family(family)) {
    return 0;
  }

  int lport = -1;
  int rport = -1;
  BPF_PROBE_READ_KERNEL_VAR(lport, &sk->__sk_common.skc_num);
  BPF_PROBE_READ_KERNEL_VAR(rport, &sk->__sk_common.skc_dport);

  struct tcp_event_t event = {};
  event.remote_addr.sa.sa_family = family;
  event.local_addr.sa.sa_family = family;

  if (family == AF_INET) {
    event.local_addr.in4.sin_port = ntohs(lport);
    event.remote_addr.in4.sin_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(event.local_addr.in4.sin_addr.s_addr, &sk->__sk_common.skc_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(event.remote_addr.in4.sin_addr.s_addr, &sk->__sk_common.skc_daddr);
  } else if (family == AF_INET6) {
    event.local_addr.in6.sin6_port = ntohs(lport);
    event.remote_addr.in6.sin6_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(event.local_addr.in6.sin6_addr, &sk->__sk_common.skc_v6_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(event.remote_addr.in6.sin6_addr, &sk->__sk_common.skc_v6_daddr);
  }

  event.timestamp_ns = bpf_ktime_get_ns();
  event.upid.tgid = tgid;
  event.upid.start_time_ticks = get_tgid_start_time();
  event.type = kTCPRx;
  event.size = copied;
  tcp_events.perf_submit(ctx, &event, sizeof(event));
  return 0;
}

int probe_entry_tcp_retransmit_skb(struct pt_regs* ctx, struct sock* skp, struct sk_buff* skb,
                                   int type) {
  if (skp == NULL) {
    return 0;
  }

  // Process id can be zero if syn packets are getting retransmitted.
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;

  uint16_t family = -1;

  BPF_PROBE_READ_KERNEL_VAR(family, &skp->__sk_common.skc_family);
  if (!valid_ip_family(family)) {
    return 0;
  }

  int lport = -1;
  int rport = -1;
  BPF_PROBE_READ_KERNEL_VAR(lport, &skp->__sk_common.skc_num);
  BPF_PROBE_READ_KERNEL_VAR(rport, &skp->__sk_common.skc_dport);

  struct tcp_event_t event = {};
  event.remote_addr.sa.sa_family = family;
  event.local_addr.sa.sa_family = family;

  if (family == AF_INET) {
    event.local_addr.in4.sin_port = ntohs(lport);
    event.remote_addr.in4.sin_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(event.local_addr.in4.sin_addr.s_addr,
                              &skp->__sk_common.skc_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(event.remote_addr.in4.sin_addr.s_addr, &skp->__sk_common.skc_daddr);
  } else if (family == AF_INET6) {
    event.local_addr.in6.sin6_port = ntohs(lport);
    event.remote_addr.in6.sin6_port = rport;
    BPF_PROBE_READ_KERNEL_VAR(event.local_addr.in6.sin6_addr, &skp->__sk_common.skc_v6_rcv_saddr);
    BPF_PROBE_READ_KERNEL_VAR(event.remote_addr.in6.sin6_addr, &skp->__sk_common.skc_v6_daddr);
  }

  event.timestamp_ns = bpf_ktime_get_ns();
  event.upid.tgid = tgid;
  event.upid.start_time_ticks = get_tgid_start_time();
  event.type = kTCPRetransmissions;
  event.size = 1;
  tcp_events.perf_submit(ctx, &event, sizeof(event));
  return 0;
}
