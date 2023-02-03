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

#pragma once

#include "src/stirling/bpf_tools/bcc_bpf_intf/go_types.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/macros.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"

// A map that communicates the location of symbols within a binary.
// This particular map has symbols that are common across golang probes.
// It is used by both go_http2_trace and go_tls_trace, and is thus included globally here.
//   Key: TGID
//   Value: Symbol addresses for the binary with that TGID.
BPF_HASH(go_common_symaddrs_map, uint32_t, struct go_common_symaddrs_t);

// Contains the registers of the golang register ABI.
// This struct is required because we use it in the regs_heap BPF map,
// which enables us to allocate this memory on the BPF heap instead of the BPF map.
struct go_regabi_regs {
  uint64_t regs[9];
};

// The BPF map used to store the registers of Go's register-based calling convention.
BPF_PERCPU_ARRAY(regs_heap, struct go_regabi_regs, 1);

// Copies the registers of the golang ABI, so that they can be
// easily accessed using an offset.
static __inline uint64_t* go_regabi_regs(const struct pt_regs* ctx) {
  uint32_t kZero = 0;
  struct go_regabi_regs* regs_heap_var = regs_heap.lookup(&kZero);
  if (regs_heap_var == NULL) {
    return NULL;
  }

#if defined(TARGET_ARCH_X86_64)
  regs_heap_var->regs[0] = ctx->ax;
  regs_heap_var->regs[1] = ctx->bx;
  regs_heap_var->regs[2] = ctx->cx;
  regs_heap_var->regs[3] = ctx->di;
  regs_heap_var->regs[4] = ctx->si;
  regs_heap_var->regs[5] = ctx->r8;
  regs_heap_var->regs[6] = ctx->r9;
  regs_heap_var->regs[7] = ctx->r10;
  regs_heap_var->regs[8] = ctx->r11;
#elif defined(TARGET_ARCH_AARCH64)
#pragma unroll
  for (uint32_t i = 0; i < 9; i++) {
    regs_heap_var->regs[i] = ctx->regs[i];
  }
#else
#error Target Architecture not supported
#endif

  return regs_heap_var->regs;
}

// Reads a golang function argument, taking into account the ABI.
// Go arguments may be in registers or on the stack.
static __inline void assign_arg(void* arg, size_t arg_size, struct location_t loc, const void* sp,
                                uint64_t* regs) {
  if (loc.type == kLocationTypeStack) {
    bpf_probe_read(arg, arg_size, sp + loc.offset);
  } else if (loc.type == kLocationTypeRegisters) {
    if (loc.offset >= 0) {
      bpf_probe_read(arg, arg_size, (char*)regs + loc.offset);
    }
  }
}

// Gets the ID of the go routine currently scheduled on the current tgid and pid.
// We do that by accessing the thread local storage (fsbase) of the current pid from the
// task_struct. From the tls, we find a pointer to the g struct and access the goid.
static inline uint64_t get_goid(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;
  struct go_common_symaddrs_t* common_symaddrs = go_common_symaddrs_map.lookup(&tgid);
  if (common_symaddrs == NULL) {
    return 0;
  }

  // Get fsbase from `struct task_struct`.
  const struct task_struct* task_ptr = (struct task_struct*)bpf_get_current_task();
  if (!task_ptr) {
    return 0;
  }

#if defined(TARGET_ARCH_X86_64)
  const void* fs_base = (void*)task_ptr->thread.fsbase;
#elif defined(TARGET_ARCH_AARCH64)
  const void* fs_base = (void*)task_ptr->thread.uw.tp_value;
#else
#error Target architecture not supported
#endif

  // Get ptr to `struct g` from 8 bytes before fsbase and then access the goID.
  uint64_t goid;
  size_t g_addr;
  bpf_probe_read_user(&g_addr, sizeof(void*), (void*)(fs_base + common_symaddrs->g_addr_offset));
  bpf_probe_read_user(&goid, sizeof(void*), (void*)(g_addr + common_symaddrs->g_goid_offset));
  return goid;
}

//-----------------------------------------------------------------------------
// FD extraction functions
//-----------------------------------------------------------------------------

// This function accesses one of the following:
//   conn.conn.conn.fd.pfd.Sysfd
//   conn.conn.fd.pfd.Sysfd
//   conn.fd.pfd.Sysfd
// The right one to use depends on the context (e.g. whether the connection uses TLS or not).
//
// (gdb) x ($sp+8)
// 0xc000069e48:  0x000000c0001560e0
// (gdb) x/2gx (0x000000c0001560e0+112)
// 0xc000156150:  0x0000000000b2b1e0  0x000000c0000caf00
// (gdb) x 0x0000000000b2b1e0
// 0xb2b1e0 <go.itab.*google.golang.org/grpc/internal/transport.bufWriter,io.Writer>:
// 0x00000000009c9400 (gdb) x/2gx (0x000000c0000caf00+40) 0xc0000caf28:  0x0000000000b3bf60
// 0x000000c00000ec20 (gdb) x 0x0000000000b3bf60 0xb3bf60
// <go.itab.*google.golang.org/grpc/credentials/internal.syscallConn,net.Conn>: 0x00000000009f66c0
// (gdb) x/2gx 0x000000c00000ec20
// 0xc00000ec20:  0x0000000000b3bea0  0x000000c000059180
// (gdb) x 0x0000000000b3bea0
// 0xb3bea0 <go.itab.*crypto/tls.Conn,net.Conn>:  0x00000000009f66c0
// (gdb) x/2gx 0x000000c000059180
// 0xc000059180:  0x0000000000b3c020  0x000000c000010048
// (gdb) x 0x0000000000b3c020
// 0xb3c020 <go.itab.*net.TCPConn,net.Conn>:  0x00000000009f66c0
//
//
// Another representation:
//   conn net.Conn
//   type net.Conn interface {
//     ...
//     data  // A pointer to *net.TCPConn, which implements the net.Conn interface.
//     type TCPConn struct {
//       conn  // conn is embedded inside TCPConn, which is defined as follows.
//       type conn struct {
//         fd *netFD
//         type netFD struct {
//           pfd poll.FD
//           type FD struct {
//             ...
//             Sysfd int
//           }
//         }
//       }
//     }
//   }
static __inline int32_t get_fd_from_conn_intf_core(struct go_interface conn_intf,
                                                   const struct go_common_symaddrs_t* symaddrs) {
  REQUIRE_SYMADDR(symaddrs->FD_Sysfd_offset, kInvalidFD);

  if (conn_intf.type == symaddrs->internal_syscallConn) {
    REQUIRE_SYMADDR(symaddrs->syscallConn_conn_offset, kInvalidFD);
    const int kSyscallConnConnOffset = 0;
    bpf_probe_read(&conn_intf, sizeof(conn_intf),
                   conn_intf.ptr + symaddrs->syscallConn_conn_offset);
  }

  if (conn_intf.type == symaddrs->tls_Conn) {
    REQUIRE_SYMADDR(symaddrs->tlsConn_conn_offset, kInvalidFD);
    bpf_probe_read(&conn_intf, sizeof(conn_intf), conn_intf.ptr + symaddrs->tlsConn_conn_offset);
  }

  if (conn_intf.type != symaddrs->net_TCPConn) {
    return kInvalidFD;
  }

  void* fd_ptr;
  bpf_probe_read(&fd_ptr, sizeof(fd_ptr), conn_intf.ptr);

  int64_t sysfd;
  bpf_probe_read(&sysfd, sizeof(int64_t), fd_ptr + symaddrs->FD_Sysfd_offset);

  return sysfd;
}
