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

// Key: SSL object pointer.
// Value: Associated TLSWrap object pointer.
BPF_HASH(node_ssl_tls_wrap_map, void*, void*);

// Tracks the currently-in-progress TLSWrap member function's this pointer, i.e., the pointer to the
// TLSWrap object.
BPF_HASH(active_TLSWrap_memfn_this, uint64_t, void*);

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
static __inline int32_t get_fd_from_tlswrap_ptr(void* tlswrap) {
  const uint64_t K_TLSWRAP_STREAMLISTENER_OFFSET = 0x78;
  const uint64_t K_STREAMLISENER_STREAM_OFFSET = 0x08;
  void* stream_ptr = tlswrap + K_TLSWRAP_STREAMLISTENER_OFFSET + K_STREAMLISENER_STREAM_OFFSET;
  void* stream = NULL;

  bpf_probe_read(&stream, sizeof(stream), stream_ptr);

  if (stream == NULL) {
    return kInvalidFD;
  }

  const uint64_t K_LIBUV_STREAM_WRAP_STREAM_BASE_OFFSET = 0x58;
  const uint64_t K_STREAM_BASE_UV_STREAM_OFFSET = 0x98;
  void* uv_stream_ptr =
      stream - K_LIBUV_STREAM_WRAP_STREAM_BASE_OFFSET + K_STREAM_BASE_UV_STREAM_OFFSET;

  void* uv_stream = NULL;
  bpf_probe_read(&uv_stream, sizeof(uv_stream), uv_stream_ptr);

  if (uv_stream == NULL) {
    return kInvalidFD;
  }

  const uint64_t K_UV_STREAM_IO_WATCHER_OFFSET = 0x88;
  const uint64_t K_UV_IO_FD_OFFSET = 0x30;
  int32_t* fd_ptr = uv_stream + K_UV_STREAM_IO_WATCHER_OFFSET + K_UV_IO_FD_OFFSET;

  int32_t fd = kInvalidFD;

  bpf_probe_read(&fd, sizeof(fd), fd_ptr);

  return fd;
}

static __inline int32_t get_fd_node(void* ssl) {
  void** tls_wrap_ptr = node_ssl_tls_wrap_map.lookup(&ssl);
  if (tls_wrap_ptr == NULL) {
    return kInvalidFD;
  }
  return get_fd_from_tlswrap_ptr(*tls_wrap_ptr);
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
