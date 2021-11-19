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

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/go_trace_common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/macros.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"

// Key: TGID
// Value: Symbol addresses for the binary with that TGID.
BPF_HASH(go_tls_symaddrs_map, uint32_t, struct go_tls_symaddrs_t);

// Probe for the crypto/tls library's write.
//
// Function signature:
//   func (c *Conn) Write(b []byte) (int, error)
//
// Symbol:
//   crypto/tls.(*Conn).Write
int probe_tls_conn_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;

  struct go_tls_symaddrs_t* symaddrs = go_tls_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->Write_c_loc, 0);
  REQUIRE_LOCATION(symaddrs->Write_b_loc, 0);
  REQUIRE_LOCATION(symaddrs->Write_retval0_loc, 0);
  REQUIRE_LOCATION(symaddrs->Write_retval1_loc, 0);

  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;

  void* conn_ptr;
  bpf_probe_read(&conn_ptr, sizeof(void*), sp + symaddrs->Write_c_loc.offset);

  char* plaintext_ptr;
  bpf_probe_read(&plaintext_ptr, sizeof(char*), sp + symaddrs->Write_b_loc.offset);

  int64_t retval0;
  bpf_probe_read(&retval0, sizeof(retval0), sp + symaddrs->Write_retval0_loc.offset);

  struct go_interface retval1;
  bpf_probe_read(&retval1, sizeof(retval1), sp + symaddrs->Write_retval1_loc.offset);

  // If function returns an error, then there's no data to trace.
  if (retval1.ptr != 0) {
    return 0;
  }

  struct go_common_symaddrs_t* common_symaddrs = go_common_symaddrs_map.lookup(&tgid);
  if (common_symaddrs == NULL) {
    return 0;
  }

  // To call get_fd_from_conn_intf, cast the conn_ptr into a go_interface.
  struct go_interface conn_intf;
  conn_intf.type = common_symaddrs->tls_Conn;
  conn_intf.ptr = conn_ptr;
  int fd = get_fd_from_conn_intf_core(conn_intf, common_symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  set_conn_as_ssl(tgid, fd);

  struct data_args_t args;
  args.source_fn = kGoTLSConnWrite;
  args.buf = plaintext_ptr;
  args.msg_len = 0;  // Unused.
  args.fd = fd;

  process_data(/* vecs */ false, ctx, id, kEgress, &args, retval0, /* ssl */ true);

  return 0;
}

// Probe for the crypto/tls library's read.
//
// Function signature:
//   func (c *Conn) Read(b []byte) (int, error)
//
// Symbol:
//   crypto/tls.(*Conn).Read
int probe_tls_conn_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;

  struct go_tls_symaddrs_t* symaddrs = go_tls_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->Read_c_loc, 0);
  REQUIRE_LOCATION(symaddrs->Read_b_loc, 0);
  REQUIRE_LOCATION(symaddrs->Read_retval0_loc, 0);
  REQUIRE_LOCATION(symaddrs->Read_retval1_loc, 0);

  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;

  void* conn_ptr;
  bpf_probe_read(&conn_ptr, sizeof(void*), sp + symaddrs->Read_c_loc.offset);

  char* plaintext_ptr;
  bpf_probe_read(&plaintext_ptr, sizeof(char*), sp + symaddrs->Read_b_loc.offset);

  int64_t retval0;
  bpf_probe_read(&retval0, sizeof(retval0), sp + symaddrs->Read_retval0_loc.offset);

  struct go_interface retval1;
  bpf_probe_read(&retval1, sizeof(retval1), sp + symaddrs->Read_retval1_loc.offset);

  // If function returns an error, then there's no data to trace.
  if (retval1.ptr != 0) {
    return 0;
  }

  // To call get_fd_from_conn_intf, cast the conn_ptr into a go_interface.
  // TODO(oazizi): Consider changing get_fd_from_conn_intf so this is not required.

  struct go_common_symaddrs_t* common_symaddrs = go_common_symaddrs_map.lookup(&tgid);
  if (common_symaddrs == NULL) {
    return 0;
  }

  struct go_interface conn_intf;
  conn_intf.type = common_symaddrs->tls_Conn;
  conn_intf.ptr = conn_ptr;
  int fd = get_fd_from_conn_intf_core(conn_intf, common_symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  set_conn_as_ssl(tgid, fd);

  struct data_args_t args;
  args.source_fn = kGoTLSConnRead;
  args.buf = plaintext_ptr;
  args.msg_len = 0;  // Unused.
  args.fd = fd;

  process_data(/* vecs */ false, ctx, id, kIngress, &args, retval0, /* ssl */ true);

  return 0;
}
