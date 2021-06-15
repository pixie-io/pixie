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
