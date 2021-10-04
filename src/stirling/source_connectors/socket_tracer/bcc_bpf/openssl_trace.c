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

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/macros.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/node_openssl_trace.c"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"

// Maps that communicates the location of symbols within a binary.
//   Key: TGID
//   Value: Symbol addresses for the binary with that TGID.
// Technically the key should be the path to a binary/shared library object instead of a TGID.
// However, we use TGID for simplicity (rather than requiring a string as a key).
// This means there will be more entries in the map, when different binaries share libssl.so.
BPF_HASH(openssl_symaddrs_map, uint32_t, struct openssl_symaddrs_t);

/***********************************************************
 * General helpers
 ***********************************************************/

static __inline void process_openssl_data(struct pt_regs* ctx, uint64_t id,
                                          const enum traffic_direction_t direction,
                                          const struct data_args_t* args) {
  // Do not change bytes_count to 'ssize_t' or 'long'.
  // Using a 64b data type for bytes_count causes negative values,
  // returned as 'int' from open-ssl, to be aliased into positive
  // values. This confuses our downstream logic in process_data().
  // This aliasing would cause process_data() to:
  // 1. process a message it should not, and
  // 2. miscalculate the expected next position (with a very large value)
  // Succinctly, DO NOT MODIFY THE DATATYPE for bytes_count.
  int bytes_count = PT_REGS_RC(ctx);
  process_data(/* vecs */ false, ctx, id, direction, args, bytes_count, /* ssl */ true);
}

/***********************************************************
 * Argument parsing helpers
 ***********************************************************/

// Given a pointer to an SSL object (i.e. the argument to functions like SSL_read),
// returns the FD of the SSL connection.
// The implementation navigates some internal structs to get this value,
// so this function may break with OpenSSL updates.
// To help combat this, the is parameterized with symbol addresses which are set by user-space,
// base on the OpenSSL version detected.
static int get_fd_symaddrs(uint32_t tgid, void* ssl) {
  struct openssl_symaddrs_t* symaddrs = openssl_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return kInvalidFD;
  }

  REQUIRE_SYMADDR(symaddrs->SSL_rbio_offset, kInvalidFD);
  REQUIRE_SYMADDR(symaddrs->RBIO_num_offset, kInvalidFD);

  // Extract FD via ssl->rbio->num.
  const void** rbio_ptr_addr = ssl + symaddrs->SSL_rbio_offset;
  const void* rbio_ptr = *rbio_ptr_addr;
  const int* rbio_num_addr = rbio_ptr + symaddrs->RBIO_num_offset;
  const int rbio_num = *rbio_num_addr;

  return rbio_num;
}

static int get_fd(uint32_t tgid, void* ssl) {
  int fd = kInvalidFD;

  // OpenSSL is used by nodejs in an asynchronous way, where the SSL_read/SSL_write functions don't
  // immediately relay the traffic to/from the socket. If we notice that this SSL call was made from
  // node, we use the FD that we obtained from a separate nodejs uprobe.
  fd = get_fd_node(tgid, ssl);
  if (fd != kInvalidFD && /*not any of the standard fds*/ fd > 2) {
    return fd;
  }

  fd = get_fd_symaddrs(tgid, ssl);
  if (fd != kInvalidFD && /*not any of the standard fds*/ fd > 2) {
    return fd;
  }

  return kInvalidFD;
}

/***********************************************************
 * BPF probe function entry-points
 ***********************************************************/

BPF_HASH(active_ssl_read_args_map, uint64_t, struct data_args_t);
BPF_HASH(active_ssl_write_args_map, uint64_t, struct data_args_t);

// Function signature being probed:
// int SSL_write(SSL *ssl, const void *buf, int num);
int probe_entry_SSL_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;

  void* ssl = (void*)PT_REGS_PARM1(ctx);
  update_node_ssl_tls_wrap_map(ssl);

  char* buf = (char*)PT_REGS_PARM2(ctx);
  int32_t fd = get_fd(tgid, ssl);

  if (fd == kInvalidFD) {
    return 0;
  }

  struct data_args_t write_args = {};
  write_args.source_fn = kSSLWrite;
  write_args.fd = fd;
  write_args.buf = buf;
  active_ssl_write_args_map.update(&id, &write_args);

  // Mark connection as SSL right away, so encrypted traffic does not get traced.
  set_conn_as_ssl(tgid, write_args.fd);

  return 0;
}

int probe_ret_SSL_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  const struct data_args_t* write_args = active_ssl_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_openssl_data(ctx, id, kEgress, write_args);
  }

  active_ssl_write_args_map.delete(&id);
  return 0;
}

// Function signature being probed:
// int SSL_read(SSL *s, void *buf, int num)
int probe_entry_SSL_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;

  void* ssl = (void*)PT_REGS_PARM1(ctx);
  update_node_ssl_tls_wrap_map(ssl);

  char* buf = (char*)PT_REGS_PARM2(ctx);
  int32_t fd = get_fd(tgid, ssl);

  if (fd == kInvalidFD) {
    return 0;
  }

  struct data_args_t read_args = {};
  read_args.source_fn = kSSLRead;
  read_args.fd = fd;
  read_args.buf = buf;
  active_ssl_read_args_map.update(&id, &read_args);

  // Mark connection as SSL right away, so encrypted traffic does not get traced.
  set_conn_as_ssl(tgid, read_args.fd);

  return 0;
}

int probe_ret_SSL_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  const struct data_args_t* read_args = active_ssl_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_openssl_data(ctx, id, kIngress, read_args);
  }

  active_ssl_read_args_map.delete(&id);
  return 0;
}
