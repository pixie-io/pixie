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

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"

// Key: SSL object pointer.
// Value: Associated TLSWrap object pointer.
BPF_HASH(node_ssl_tls_wrap_map, void*, void*);

// Tracks the currently-in-progress TLSWrap member function's this pointer, i.e., the pointer to the
// TLSWrap object.
BPF_HASH(active_TLSWrap_memfn_this, uint64_t, void*);

// Maps that communicates the offsets of symbols related to reading file descriptor from TLSWrap
// pointer of a node executable.
//   Key: TGID
//   Value: Symbol offsets for the node executable with that TGID.
//
// Userspace examines the nodejs process' executable's version to write the corresponding offsets to
// this map.
BPF_HASH(node_tlswrap_symaddrs_map, uint32_t, struct node_tlswrap_symaddrs_t);

static __inline void* get_tls_wrap_for_memfn() {
  uint64_t id = bpf_get_current_pid_tgid();
  void** args = active_TLSWrap_memfn_this.lookup(&id);
  if (args == NULL) {
    return NULL;
  }
  return *args;
}

static __inline void update_node_ssl_tls_wrap_map(void* ssl) {
  void* tls_wrap = get_tls_wrap_for_memfn();
  if (tls_wrap == NULL) {
    return;
  }

  // The TLSWrap object associated with SSL object might change. So even if node_ssl_tls_wrap_map
  // might already have value for &ssl, we still blindly update to the new one.
  // TODO(yzhao): Investigate how SSL can associate with different TLSWrap objects.
  node_ssl_tls_wrap_map.update(&ssl, &tls_wrap);
}

// Reads fd by chasing pointers fields and offsets starting from the pointer to a TLSWrap object.
// TODO(yzhao): Add a doc to explain the layout of the data structures.
static __inline int32_t get_fd_from_tlswrap_ptr(const struct node_tlswrap_symaddrs_t* symaddrs,
                                                void* tlswrap) {
  void* stream_ptr =
      tlswrap + symaddrs->TLSWrap_StreamListener_offset + symaddrs->StreamListener_stream_offset;
  void* stream = NULL;

  bpf_probe_read(&stream, sizeof(stream), stream_ptr);

  if (stream == NULL) {
    return kInvalidFD;
  }

  void* uv_stream_ptr = stream - symaddrs->StreamBase_StreamResource_offset -
                        symaddrs->LibuvStreamWrap_StreamBase_offset +
                        symaddrs->LibuvStreamWrap_stream_offset;

  void* uv_stream = NULL;
  bpf_probe_read(&uv_stream, sizeof(uv_stream), uv_stream_ptr);

  if (uv_stream == NULL) {
    return kInvalidFD;
  }

  int32_t* fd_ptr =
      uv_stream + symaddrs->uv_stream_s_io_watcher_offset + symaddrs->uv__io_s_fd_offset;

  int32_t fd = kInvalidFD;

  if (bpf_probe_read(&fd, sizeof(fd), fd_ptr) != 0) {
    return kInvalidFD;
  }

  return fd;
}

// TLSWrap holds a pointer to a SSL object. SSL object does not store the file descriptor used for
// socket IO, TLSWrap does. So the overall process of obtaining the file descriptor works as
// follows:
// * Get TLSWrap object's address from node_ssl_tls_wrap_map
// * Get FD from TLSWrap object, with fields' offsets from userspace
static __inline int32_t get_fd_node(uint32_t tgid, void* ssl) {
  void** tls_wrap_ptr = node_ssl_tls_wrap_map.lookup(&ssl);
  if (tls_wrap_ptr == NULL) {
    return kInvalidFD;
  }

  const struct node_tlswrap_symaddrs_t* symaddrs = node_tlswrap_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return kInvalidFD;
  }

  return get_fd_from_tlswrap_ptr(symaddrs, *tls_wrap_ptr);
}

// SSL_new is invoked by TLSWrap::TLSWrap(). Its return value is used to update the map.
int probe_ret_SSL_new(struct pt_regs* ctx) {
  void* ssl = (void*)PT_REGS_RC(ctx);
  if (ssl == NULL) {
    return 0;
  }
  update_node_ssl_tls_wrap_map(ssl);
  return 0;
}

// This pair of probe functions are attached to TLSWrap member functions to cache the TLSWrap object
// pointer, so that the probes on their nested functions can retrieve the pointer.
int probe_entry_TLSWrap_memfn(struct pt_regs* ctx) {
  void* tls_wrap = (void*)PT_REGS_PARM1(ctx);
  uint64_t id = bpf_get_current_pid_tgid();
  active_TLSWrap_memfn_this.update(&id, &tls_wrap);
  return 0;
}

int probe_ret_TLSWrap_memfn(struct pt_regs* ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  active_TLSWrap_memfn_this.delete(&id);
  return 0;
}
