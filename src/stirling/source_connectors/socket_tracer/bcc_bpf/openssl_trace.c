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

BPF_ARRAY(openssl_trace_state, int, 1);
BPF_HASH(openssl_trace_state_debug, uint32_t, struct openssl_trace_state_debug_t);

/***********************************************************
 * General helpers
 ***********************************************************/

static __inline void process_openssl_data(struct pt_regs* ctx, uint64_t id,
                                          const enum traffic_direction_t direction,
                                          const struct data_args_t* args, bool is_ex_call) {
  // Do not change bytes_count to 'ssize_t' or 'long'.
  // Using a 64b data type for bytes_count causes negative values,
  // returned as 'int' from open-ssl, to be aliased into positive
  // values. This confuses our downstream logic in process_data().
  // This aliasing would cause process_data() to:
  // 1. process a message it should not, and
  // 2. miscalculate the expected next position (with a very large value)
  // Succinctly, DO NOT MODIFY THE DATATYPE for bytes_count.
  int bytes_count = PT_REGS_RC(ctx);

  // SSL_write_ex and SSL_read_ex will return 1 on success, which means that
  // all requested application data bytes have been written to the SSL connection.
  // The number of bytes read or written will be contained in the pointer passed
  // as the fourth argument to the function.
  if (bytes_count == 1 && args->ssl_ex_len != NULL) {
    size_t ex_bytes;
    BPF_PROBE_READ_VAR(ex_bytes, args->ssl_ex_len);
    bytes_count = ex_bytes;
  }
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

static __inline int get_fd_and_eval_nested_syscall_detection(uint64_t pid_tgid) {
  struct nested_syscall_fd_t* nested_syscall_fd_ptr = ssl_user_space_call_map.lookup(&pid_tgid);

  if (nested_syscall_fd_ptr == NULL) {
    // This state should not occur since ssl_user_space_call_map for a given pid_tgid is set
    // upon SSL_write and SSL_read entry.
    return kInvalidFD;
  }

  bool mismatched_fds = nested_syscall_fd_ptr->mismatched_fds;

  if (ENABLE_TLS_DEBUG_SOURCES || mismatched_fds) {
    int error_status_idx = kOpenSSLTraceStatusIdx;
    int status_code = mismatched_fds ? kOpenSSLMismatchedFDsDetected : kOpenSSLTraceOk;
    uint32_t tgid = pid_tgid >> 32;
    enum ssl_source_t ssl_source = get_ssl_source(tgid);

    struct openssl_trace_state_debug_t debug = {};
    bpf_get_current_comm(&debug.comm, sizeof(debug.comm));
    debug.ssl_source = ssl_source;
    debug.mismatched_fd = mismatched_fds;

    struct conn_info_t* conn_info = get_or_create_conn_info(tgid, nested_syscall_fd_ptr->fd);
    debug.protocol = conn_info != NULL ? conn_info->protocol : kProtocolUnknown;

    openssl_trace_state.update(&error_status_idx, &status_code);
    openssl_trace_state_debug.update(&tgid, &debug);
  }

  int fd = nested_syscall_fd_ptr->fd;
  ssl_user_space_call_map.delete(&pid_tgid);

  return fd;
}

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
  set_conn_as_ssl(tgid, write_args.fd, get_ssl_source(tgid));

  return 0;
}

int probe_ret_SSL_write(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  const struct data_args_t* write_args = active_ssl_write_args_map.lookup(&id);
  if (write_args != NULL) {
    process_openssl_data(ctx, id, kEgress, write_args, false);
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
  set_conn_as_ssl(tgid, read_args.fd, get_ssl_source(tgid));

  return 0;
}

int probe_ret_SSL_read(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();

  const struct data_args_t* read_args = active_ssl_read_args_map.lookup(&id);
  if (read_args != NULL) {
    process_openssl_data(ctx, id, kIngress, read_args, false);
  }

  active_ssl_read_args_map.delete(&id);
  return 0;
}

static int probe_entry_SSL_write_syscall_fd_access_agnostic(struct pt_regs* ctx, bool is_ex_call) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;

  void* ssl = (void*)PT_REGS_PARM1(ctx);

  struct nested_syscall_fd_t nested_syscall_fd = {
      .fd = kInvalidFD,
  };
  ssl_user_space_call_map.update(&id, &nested_syscall_fd);

  char* buf = (char*)PT_REGS_PARM2(ctx);

  struct data_args_t write_args = {};
  write_args.source_fn = kSSLWrite;
  write_args.buf = buf;
  if (is_ex_call) {
    size_t* ssl_ex_len = (size_t*)PT_REGS_PARM4(ctx);
    write_args.ssl_ex_len = ssl_ex_len;
  }

  active_ssl_write_args_map.update(&id, &write_args);

  return 0;
}

// Function signature being probed:
// int SSL_write(SSL *ssl, const void *buf, int num);
int probe_entry_SSL_write_syscall_fd_access(struct pt_regs* ctx) {
  return probe_entry_SSL_write_syscall_fd_access_agnostic(ctx, false);
}

// Function signature being probed:
// int SSL_write_ex(SSL *ssl, const void *buf, size_t* written);
//
// On success SSL_write_ex will store the number of bytes written in *written.
int probe_entry_SSL_write_ex_syscall_fd_access(struct pt_regs* ctx) {
  return probe_entry_SSL_write_syscall_fd_access_agnostic(ctx, true);
}

static int probe_ret_SSL_write_syscall_fd_access_agnostic(struct pt_regs* ctx, bool is_ex_call) {
  uint64_t id = bpf_get_current_pid_tgid();

  int fd = get_fd_and_eval_nested_syscall_detection(id);
  if (fd == kInvalidFD) {
    return 0;
  }

  struct data_args_t* write_args = active_ssl_write_args_map.lookup(&id);
  // Set socket fd
  if (write_args != NULL) {
    write_args->fd = fd;
    process_openssl_data(ctx, id, kEgress, write_args, is_ex_call);
  }

  active_ssl_write_args_map.delete(&id);
  return 0;
}

int probe_ret_SSL_write_ex_syscall_fd_access(struct pt_regs* ctx) {
  return probe_ret_SSL_write_syscall_fd_access_agnostic(ctx, true);
}

int probe_ret_SSL_write_syscall_fd_access(struct pt_regs* ctx) {
  return probe_ret_SSL_write_syscall_fd_access_agnostic(ctx, false);
}

static int probe_entry_SSL_read_syscall_fd_access_agnostic(struct pt_regs* ctx, bool is_ex_call) {
  uint64_t id = bpf_get_current_pid_tgid();
  uint32_t tgid = id >> 32;

  void* ssl = (void*)PT_REGS_PARM1(ctx);

  struct nested_syscall_fd_t nested_syscall_fd = {
      .fd = kInvalidFD,
  };
  ssl_user_space_call_map.update(&id, &nested_syscall_fd);

  char* buf = (char*)PT_REGS_PARM2(ctx);

  struct data_args_t read_args = {};
  read_args.source_fn = kSSLRead;
  read_args.buf = buf;
  if (is_ex_call) {
    size_t* ssl_ex_len = (size_t*)PT_REGS_PARM4(ctx);
    read_args.ssl_ex_len = ssl_ex_len;
  }

  active_ssl_read_args_map.update(&id, &read_args);

  return 0;
}

// Function signature being probed:
// int SSL_read(SSL *s, void *buf, int num)
int probe_entry_SSL_read_syscall_fd_access(struct pt_regs* ctx) {
  return probe_entry_SSL_read_syscall_fd_access_agnostic(ctx, false);
}

// Function signature being probed:
// int SSL_read_ex(SSL *ssl, void *buf, size_t num, size_t *readbytes);
//
// On success SSL_read_ex will store the number of bytes actually read in *readbytes.
int probe_entry_SSL_read_ex_syscall_fd_access(struct pt_regs* ctx) {
  return probe_entry_SSL_read_syscall_fd_access_agnostic(ctx, true);
}

static int probe_ret_SSL_read_syscall_fd_access_agnostic(struct pt_regs* ctx, bool is_ex_call) {
  uint64_t id = bpf_get_current_pid_tgid();

  int fd = get_fd_and_eval_nested_syscall_detection(id);
  if (fd == kInvalidFD) {
    return 0;
  }

  struct data_args_t* read_args = active_ssl_read_args_map.lookup(&id);
  if (read_args != NULL) {
    read_args->fd = fd;
    process_openssl_data(ctx, id, kIngress, read_args, is_ex_call);
  }

  active_ssl_read_args_map.delete(&id);
  return 0;
}

int probe_ret_SSL_read_ex_syscall_fd_access(struct pt_regs* ctx) {
  return probe_ret_SSL_read_syscall_fd_access_agnostic(ctx, true);
}

int probe_ret_SSL_read_syscall_fd_access(struct pt_regs* ctx) {
  return probe_ret_SSL_read_syscall_fd_access_agnostic(ctx, false);
}
