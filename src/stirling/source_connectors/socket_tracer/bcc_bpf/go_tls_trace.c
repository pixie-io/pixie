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
  REQUIRE_SYMADDR(symaddrs->Write_c_offset, 0);
  REQUIRE_SYMADDR(symaddrs->Write_b_offset, 0);

  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  const char* sp = (const char*)ctx->sp;

  void* conn_ptr;
  bpf_probe_read(&conn_ptr, sizeof(void*), sp + symaddrs->Write_c_offset);

  struct go_byte_array plaintext;
  bpf_probe_read(&plaintext, sizeof(struct go_byte_array), sp + symaddrs->Write_b_offset);

  // TODO(oazizi): Use symaddrs instead of constant offsets.
  int64_t retval0;
  bpf_probe_read(&retval0, sizeof(retval0), sp + 40);

  struct go_interface retval1;
  bpf_probe_read(&retval1, sizeof(retval1), sp + 48);

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
  args.buf = plaintext.ptr;
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
  REQUIRE_SYMADDR(symaddrs->Read_c_offset, 0);
  REQUIRE_SYMADDR(symaddrs->Read_b_offset, 0);

  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  const char* sp = (const char*)ctx->sp;

  void* conn_ptr;
  bpf_probe_read(&conn_ptr, sizeof(void*), sp + symaddrs->Read_c_offset);

  struct go_byte_array plaintext;
  bpf_probe_read(&plaintext, sizeof(struct go_byte_array), sp + symaddrs->Read_b_offset);

  int64_t retval0;
  bpf_probe_read(&retval0, sizeof(retval0), sp + 40);

  struct go_interface retval1;
  bpf_probe_read(&retval1, sizeof(retval1), sp + 48);

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
  args.buf = plaintext.ptr;
  args.msg_len = 0;  // Unused.
  args.fd = fd;

  process_data(/* vecs */ false, ctx, id, kIngress, &args, retval0, /* ssl */ true);

  return 0;
}
