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

#include "src/stirling/bpf_tools/bcc_bpf/utils.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/go_trace_common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/macros.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/symaddrs.h"

#define MAX_HEADER_COUNT 59

BPF_PERF_OUTPUT(go_grpc_events);

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(data_event_buffer_heap, struct go_grpc_data_event_t, 1);
static __inline struct go_grpc_data_event_t* get_data_event() {
  uint32_t kZero = 0;
  return data_event_buffer_heap.lookup(&kZero);
}

BPF_PERCPU_ARRAY(header_event_buffer_heap, struct go_grpc_http2_header_event_t, 1);
static __inline struct go_grpc_http2_header_event_t* get_header_event() {
  uint32_t kZero = 0;
  return header_event_buffer_heap.lookup(&kZero);
}

// Maps that communicates the location of symbols within a binary.
//   Key: TGID
//   Value: Symbol addresses for the binary with that TGID.
BPF_HASH(http2_symaddrs_map, uint32_t, struct go_http2_symaddrs_t);

// This map is used to help extract HTTP2 headers from the net/http library.
// The tracing process requires multiple probes:
//  - The primary probe collects context and sets this map entry.
//  - Dependent probes trace functions called by the primary function;
//    these read the map to get the context.
//  - The return probe of the primary function deletes the map entry.
//
// Key: encoder instance pointer
// Value: Header attributes (e.g. stream_id, fd)
BPF_HASH(active_write_headers_frame_map, void*, struct go_grpc_event_attr_t);

// Meaning of flag bits in FrameHeader flags.
// https://github.com/golang/net/blob/master/http2/frame.go
// TODO(oazizi): Use DWARF info to get these values.
const uint8_t kFlagDataEndStream = 0x1;
const uint8_t kFlagHeadersEndStream = 0x1;

// A golang array consists of a pointer, a length and a capacity.
// These could come from DWARF information, but are hard-coded,
// since an array is a pretty stable type.
const int kGoArrayPtrOffset = 0;
const int kGoArrayLenOffset = 8;
const int kGoArrayCapOffset = 16;

//-----------------------------------------------------------------------------
// FD extraction functions
//-----------------------------------------------------------------------------

static __inline int32_t get_fd_from_conn_intf(struct go_interface conn_intf) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_common_symaddrs_t* symaddrs = go_common_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  return get_fd_from_conn_intf_core(conn_intf, symaddrs);
}

static __inline int32_t get_fd_from_io_writer_intf(void* io_writer_intf_ptr) {
  // At this point, we have something like the following struct:
  // io.Writer(*crypto/tls.Conn)
  //
  // Note that it is an io.Writer interface, not a net.Conn interface.
  // In this case, it is implemented by tls.Conn, which could fit either io.Writer or net.Conn.
  // Since it is not a net.Conn interface, we need to perform an extra dereference to get
  // to a net.Conn interface that we can examine for the FD.
  // It may be possible that is implemented by some other Conn type,
  // but this code only works for tls.Conn.
  // Still have to figure out how golang figures this out dynamically, given that
  // we're not seeing the expected interface type.

  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_common_symaddrs_t* symaddrs = go_common_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  REQUIRE_SYMADDR(symaddrs->tlsConn_conn_offset, kInvalidFD);

  struct go_interface conn_intf;
  BPF_PROBE_READ_VAR(conn_intf, io_writer_intf_ptr + symaddrs->tlsConn_conn_offset);

  return get_fd_from_conn_intf_core(conn_intf, symaddrs);
}

// Returns the file descriptor from a http2.Framer object.
static __inline int32_t get_fd_from_http2_Framer(const void* framer_ptr,
                                                 const struct go_http2_symaddrs_t* symaddrs) {
  REQUIRE_SYMADDR(symaddrs->Framer_w_offset, kInvalidFD);
  REQUIRE_SYMADDR(symaddrs->bufWriter_conn_offset, kInvalidFD);

  struct go_interface io_writer_interface;
  BPF_PROBE_READ_VAR(io_writer_interface, framer_ptr + symaddrs->Framer_w_offset);

  // At this point, we have the following struct:
  // go.itab.*google.golang.org/grpc/internal/transport.bufWriter,io.Writer
  if (io_writer_interface.type != symaddrs->transport_bufWriter) {
    return kInvalidFD;
  }

  struct go_interface conn_intf;
  BPF_PROBE_READ_VAR(conn_intf, io_writer_interface.ptr + symaddrs->bufWriter_conn_offset);

  return get_fd_from_conn_intf(conn_intf);
}

// Returns the file descriptor from a http.http2Framer object.
// Essentially accesses framer_ptr.w.w.conn.
static __inline int32_t get_fd_from_http_http2Framer(const void* framer_ptr,
                                                     const struct go_http2_symaddrs_t* symaddrs) {
  REQUIRE_SYMADDR(symaddrs->http2Framer_w_offset, kInvalidFD);
  REQUIRE_SYMADDR(symaddrs->http2bufferedWriter_w_offset, kInvalidFD);

  struct go_interface io_writer_interface;
  BPF_PROBE_READ_VAR(io_writer_interface, framer_ptr + symaddrs->http2Framer_w_offset);

  // At this point, we have the following struct:
  // go.itab.*net/http.http2bufferedWriter,io.Writer
  if (io_writer_interface.type != symaddrs->http_http2bufferedWriter) {
    return kInvalidFD;
  }

  struct go_interface inner_io_writer_interface;
  BPF_PROBE_READ_VAR(inner_io_writer_interface,
                     io_writer_interface.ptr + symaddrs->http2bufferedWriter_w_offset);

  return get_fd_from_io_writer_intf(inner_io_writer_interface.ptr);
}

//-----------------------------------------------------------------------------
// HTTP2 Header Tracing Functions
//-----------------------------------------------------------------------------

static __inline void copy_header_field(struct header_field_t* dst, struct gostring* src) {
  if (src->len <= 0) {
    dst->size = 0;
    return;
  }
  dst->size = min_int64_t(src->len, (int64_t)HEADER_FIELD_STR_SIZE);
  bpf_probe_read(dst->msg, dst->size, src->ptr);
}

static __inline void fill_header_field(struct go_grpc_http2_header_event_t* event,
                                       const void* header_field_ptr,
                                       const struct go_http2_symaddrs_t* symaddrs) {
  struct gostring name;
  BPF_PROBE_READ_VAR(name, header_field_ptr + symaddrs->HeaderField_Name_offset);

  struct gostring value;
  BPF_PROBE_READ_VAR(value, header_field_ptr + symaddrs->HeaderField_Value_offset);

  copy_header_field(&event->name, &name);
  copy_header_field(&event->value, &value);
}

static __inline void submit_headers(struct pt_regs* ctx, enum http2_probe_type_t probe_type,
                                    enum grpc_event_type_t type, int32_t fd, uint32_t stream_id,
                                    bool end_stream, void* fields_ptr, int64_t fields_len,
                                    const struct go_http2_symaddrs_t* symaddrs) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }

  struct go_grpc_http2_header_event_t* event = get_header_event();
  if (event == NULL) {
    return;
  }

  event->attr.probe_type = probe_type;
  event->attr.event_type = type;
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.conn_id = conn_info->conn_id;
  event->attr.stream_id = stream_id;
  event->attr.end_stream = false;

  // TODO(oazizi): Replace this constant with information from DWARF.
  const int kSizeOfHeaderField = 40;
#pragma unroll
  for (unsigned int i = 0; i < MAX_HEADER_COUNT; ++i) {
    if (i < fields_len) {
      fill_header_field(event, fields_ptr + i * kSizeOfHeaderField, symaddrs);
      go_grpc_events.perf_submit(ctx, event, sizeof(*event));
    }
  }

  // If end of stream, send one extra empty header with end-stream flag set.
  if (end_stream) {
    event->name.size = 0;
    event->value.size = 0;
    event->attr.end_stream = true;
    go_grpc_events.perf_submit(ctx, event, sizeof(*event));
  }
}

static __inline void submit_header(struct pt_regs* ctx, enum http2_probe_type_t probe_type,
                                   enum grpc_event_type_t type, void* encoder_ptr,
                                   struct gostring* name_ptr, struct gostring* value_ptr,
                                   const struct go_http2_symaddrs_t* symaddrs) {
  struct go_grpc_event_attr_t* attr = active_write_headers_frame_map.lookup(&encoder_ptr);
  if (attr == NULL) {
    return;
  }

  struct go_grpc_http2_header_event_t* event = get_header_event();
  if (event == NULL) {
    return;
  }

  event->attr.probe_type = probe_type;
  event->attr.event_type = type;
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.conn_id = attr->conn_id;
  event->attr.stream_id = attr->stream_id;
  event->attr.end_stream = false;

  copy_header_field(&event->name, name_ptr);
  copy_header_field(&event->value, value_ptr);

  go_grpc_events.perf_submit(ctx, event, sizeof(*event));
}

// TODO(oazizi): Remove this struct; Use DWARF instead.
struct go_grpc_framer_t {
  void* writer;
  void* http2_framer;
};

// Probes (*loopyWriter).writeHeader() inside gRPC-go, which writes HTTP2 headers to the server.
//
// Function signature:
//     func (l *loopyWriter) writeHeader(streamID uint32, endStream bool, hf []hpack.HeaderField,
//     onWrite func()) error
//
// Symbol:
//   google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader
int probe_loopy_writer_write_header(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->writeHeader_l_loc, 0);
  REQUIRE_LOCATION(symaddrs->writeHeader_streamID_loc, 0);
  REQUIRE_LOCATION(symaddrs->writeHeader_endStream_loc, 0);
  REQUIRE_LOCATION(symaddrs->writeHeader_hf_ptr_loc, 0);
  REQUIRE_LOCATION(symaddrs->writeHeader_hf_len_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->loopyWriter_framer_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* loopy_writer_ptr = NULL;
  assign_arg(&loopy_writer_ptr, sizeof(loopy_writer_ptr), symaddrs->writeHeader_l_loc, sp, regs);

  uint32_t stream_id = 0;
  assign_arg(&stream_id, sizeof(stream_id), symaddrs->writeHeader_streamID_loc, sp, regs);

  bool end_stream = false;
  assign_arg(&end_stream, sizeof(end_stream), symaddrs->writeHeader_endStream_loc, sp, regs);

  void* fields_ptr = NULL;
  assign_arg(&fields_ptr, sizeof(fields_ptr), symaddrs->writeHeader_hf_ptr_loc, sp, regs);

  int64_t fields_len = 0;
  assign_arg(&fields_len, sizeof(fields_len), symaddrs->writeHeader_hf_len_loc, sp, regs);

  // ---------------------------------------------
  // Extract members
  // ---------------------------------------------

  void* framer_ptr;
  BPF_PROBE_READ_VAR(framer_ptr, loopy_writer_ptr + symaddrs->loopyWriter_framer_offset);

  // TODO(oazizi): Stop using mirrored go structs, and use DWARF info instead.
  struct go_grpc_framer_t go_grpc_framer;
  BPF_PROBE_READ_VAR(go_grpc_framer, framer_ptr);

  const int32_t fd = get_fd_from_http2_Framer(go_grpc_framer.http2_framer, symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  submit_headers(ctx, k_probe_loopy_writer_write_header, kHeaderEventWrite, fd, stream_id,
                 end_stream, fields_ptr, fields_len, symaddrs);

  return 0;
}

// Shared helper function for:
//   probe_http2_client_operate_headers()
//   probe_http2_server_operate_headers()
// The two probes are similar but the conn_intf location is specific to each struct.
// MetaHeadersFrame_ptr is of type: golang.org/x/net/http2.MetaHeadersFrame
static __inline void probe_http2_operate_headers(struct pt_regs* ctx,
                                                 enum http2_probe_type_t probe_type, int32_t fd,
                                                 const void* MetaHeadersFrame_ptr,
                                                 const struct go_http2_symaddrs_t* symaddrs) {
  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->MetaHeadersFrame_HeadersFrame_offset, /* none */);
  REQUIRE_SYMADDR(symaddrs->MetaHeadersFrame_Fields_offset, /* none */);
  REQUIRE_SYMADDR(symaddrs->HeadersFrame_FrameHeader_offset, /* none */);
  REQUIRE_SYMADDR(symaddrs->FrameHeader_Flags_offset, /* none */);
  REQUIRE_SYMADDR(symaddrs->FrameHeader_StreamID_offset, /* none */);

  // ------------------------------------------------------
  // Extract members of MetaHeadersFrame_ptr (HeadersFrame, Fields)
  // ------------------------------------------------------

  void* HeadersFrame_ptr;
  BPF_PROBE_READ_VAR(HeadersFrame_ptr,
                     MetaHeadersFrame_ptr + symaddrs->MetaHeadersFrame_HeadersFrame_offset);

  void* fields_ptr;
  BPF_PROBE_READ_VAR(fields_ptr, MetaHeadersFrame_ptr + symaddrs->MetaHeadersFrame_Fields_offset +
                                     kGoArrayPtrOffset);

  int64_t fields_len;
  BPF_PROBE_READ_VAR(fields_len, MetaHeadersFrame_ptr + symaddrs->MetaHeadersFrame_Fields_offset +
                                     kGoArrayLenOffset);

  int64_t fields_cap;
  BPF_PROBE_READ_VAR(fields_cap, MetaHeadersFrame_ptr + symaddrs->MetaHeadersFrame_Fields_offset +
                                     kGoArrayCapOffset);

  // ------------------------------------------------------
  // Extract members of HeadersFrame_ptr (HeadersFrame)
  // ------------------------------------------------------

  void* FrameHeader_ptr = HeadersFrame_ptr + symaddrs->HeadersFrame_FrameHeader_offset;

  // ------------------------------------------------------
  // Extract members of FrameHeader_ptr (stream_id, end_stream)
  // ------------------------------------------------------

  uint8_t flags;
  bpf_probe_read(&flags, sizeof(uint8_t), FrameHeader_ptr + symaddrs->FrameHeader_Flags_offset);
  const bool end_stream = flags & kFlagHeadersEndStream;

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t),
                 FrameHeader_ptr + symaddrs->FrameHeader_StreamID_offset);

  // ------------------------------------------------------
  // Submit
  // ------------------------------------------------------

  // TODO(yzhao): We saw some arbitrary large slices received by operateHeaders(), it's not clear
  // what conditions result into them.
  if (fields_len > 100 || fields_len <= 0 || fields_cap <= 0) {
    return;
  }

  submit_headers(ctx, probe_type, kHeaderEventRead, fd, stream_id, end_stream, fields_ptr,
                 fields_len, symaddrs);
}

// Probe for the golang.org/x/net/http2 library's header reader (client-side).
//
// Probes (*http2Client).operateHeaders(*http2.MetaHeadersFrame) inside gRPC-go, which processes
// HTTP2 headers of the received responses.
//
// Function signature:
//   func (t *http2Client) operateHeaders(frame *http2.MetaHeadersFrame)
//
// Symbol:
//   google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders
int probe_http2_client_operate_headers(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->http2Client_operateHeaders_t_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Client_operateHeaders_frame_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->http2Client_conn_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* http2_client_ptr = NULL;
  assign_arg(&http2_client_ptr, sizeof(http2_client_ptr),
             symaddrs->http2Client_operateHeaders_t_loc, sp, regs);

  void* frame_ptr = NULL;
  assign_arg(&frame_ptr, sizeof(frame_ptr), symaddrs->http2Client_operateHeaders_frame_loc, sp,
             regs);

  // ---------------------------------------------
  // Extract members
  // ---------------------------------------------

  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf),
                 http2_client_ptr + symaddrs->http2Client_conn_offset);

  const int32_t fd = get_fd_from_conn_intf(conn_intf);
  if (fd == kInvalidFD) {
    return 0;
  }

  probe_http2_operate_headers(ctx, k_probe_http2_client_operate_headers, fd, frame_ptr, symaddrs);

  return 0;
}

// Probe for the golang.org/x/net/http2 library's header reader (server-side).
//
// Function signature:
//   func (t *http2Server) operateHeaders(frame *http2.MetaHeadersFrame, handle func(*Stream),
//                                        traceCtx func(context.Context, string) context.Context
//                                        (fatal bool)
// Symbol:
//   google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders
int probe_http2_server_operate_headers(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->http2Server_operateHeaders_t_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Server_operateHeaders_frame_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->http2Server_conn_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* http2_server_ptr = NULL;
  assign_arg(&http2_server_ptr, sizeof(http2_server_ptr),
             symaddrs->http2Server_operateHeaders_t_loc, sp, regs);

  void* frame_ptr = NULL;
  assign_arg(&frame_ptr, sizeof(frame_ptr), symaddrs->http2Server_operateHeaders_frame_loc, sp,
             regs);

  // ---------------------------------------------
  // Extract members
  // ---------------------------------------------

  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf),
                 http2_server_ptr + symaddrs->http2Server_conn_offset);

  const int32_t fd = get_fd_from_conn_intf(conn_intf);
  if (fd == kInvalidFD) {
    return 0;
  }

  probe_http2_operate_headers(ctx, k_probe_http2_server_operate_headers, fd, frame_ptr, symaddrs);

  return 0;
}

// Probe for the net/http library's header reader.
//
// Function signature:
//   func (sc *http2serverConn) processHeaders(f *http2MetaHeadersFrame) error
//
// Symbol:
//   net/http.(*http2serverConn).processHeaders
//
// Verified to be stable from go1.?? to t go.1.13.
int probe_http_http2serverConn_processHeaders(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->processHeaders_sc_loc, 0);
  REQUIRE_LOCATION(symaddrs->processHeaders_f_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->http2MetaHeadersFrame_http2HeadersFrame_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2MetaHeadersFrame_Fields_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2HeadersFrame_http2FrameHeader_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2FrameHeader_Flags_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2FrameHeader_StreamID_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2serverConn_conn_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* http2serverConn_ptr = NULL;
  assign_arg(&http2serverConn_ptr, sizeof(http2serverConn_ptr), symaddrs->processHeaders_sc_loc, sp,
             regs);

  void* http2MetaHeadersFrame_ptr = NULL;
  assign_arg(&http2MetaHeadersFrame_ptr, sizeof(http2MetaHeadersFrame_ptr),
             symaddrs->processHeaders_f_loc, sp, regs);

  // ------------------------------------------------------
  // Extract members of http2MetaHeadersFrame_ptr (headers)
  // ------------------------------------------------------

  void* fields_ptr;
  bpf_probe_read(&fields_ptr, sizeof(void*),
                 http2MetaHeadersFrame_ptr + symaddrs->http2MetaHeadersFrame_Fields_offset +
                     kGoArrayPtrOffset);

  int64_t fields_len;
  bpf_probe_read(&fields_len, sizeof(int64_t),
                 http2MetaHeadersFrame_ptr + symaddrs->http2MetaHeadersFrame_Fields_offset +
                     kGoArrayLenOffset);

  void* http2HeadersFrame_ptr;
  bpf_probe_read(
      &http2HeadersFrame_ptr, sizeof(void*),
      http2MetaHeadersFrame_ptr + symaddrs->http2MetaHeadersFrame_http2HeadersFrame_offset);

  // ------------------------------------------------------
  // Extract members of http2HeadersFrame_ptr (stream_id, end_stream)
  // ------------------------------------------------------

  void* http2FrameHeader_ptr =
      http2HeadersFrame_ptr + symaddrs->http2HeadersFrame_http2FrameHeader_offset;

  uint8_t flags;
  bpf_probe_read(&flags, sizeof(uint8_t),
                 http2FrameHeader_ptr + symaddrs->http2FrameHeader_Flags_offset);
  const bool end_stream = flags & kFlagHeadersEndStream;

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t),
                 http2FrameHeader_ptr + symaddrs->http2FrameHeader_StreamID_offset);

  // ------------------------------------------------------
  // Extract members of http2serverConn_ptr (fd)
  // ------------------------------------------------------

  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf),
                 http2serverConn_ptr + symaddrs->http2serverConn_conn_offset);

  const int32_t fd = get_fd_from_conn_intf(conn_intf);
  if (fd == kInvalidFD) {
    return 0;
  }

  // ------------------------------------------------------
  // Wrap-ups
  // ------------------------------------------------------

  submit_headers(ctx, k_probe_http_http2serverConn_processHeaders, kHeaderEventRead, fd, stream_id,
                 end_stream, fields_ptr, fields_len, symaddrs);

  return 0;
}

// Probe for the hpack's header encoder.
//
// Function signature:
//   func (e *Encoder) WriteField(f HeaderField) error
//
// Symbol:
//   golang.org/x/net/http2/hpack.(*Encoder).WriteField
//
// Verified to be stable from at least go1.6 to t go.1.13.
int probe_hpack_header_encoder(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->WriteField_e_loc, 0);
  REQUIRE_LOCATION(symaddrs->WriteField_f_name_loc, 0);
  REQUIRE_LOCATION(symaddrs->WriteField_f_value_loc, 0);

  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* encoder_ptr = NULL;
  assign_arg(&encoder_ptr, sizeof(encoder_ptr), symaddrs->WriteField_e_loc, sp, regs);

  struct gostring name = {};
  assign_arg(&name, sizeof(struct gostring), symaddrs->WriteField_f_name_loc, sp, regs);

  struct gostring value = {};
  assign_arg(&value, sizeof(struct gostring), symaddrs->WriteField_f_value_loc, sp, regs);

  // ------------------------------------------------------
  // Process
  // ------------------------------------------------------

  submit_header(ctx, k_probe_hpack_header_encoder, kHeaderEventWrite, encoder_ptr, &name, &value,
                symaddrs);

  return 0;
}

// Probe for the net/http library's header writer.
//
// Function signature:
//   func (w *http2writeResHeaders) writeFrame(ctx http2writeContext) error {
//
// Symbol:
//   net/http.(*http2writeResHeaders).writeFrame
//
// Verified to be stable from go1.?? to t go.1.13.
int probe_http_http2writeResHeaders_write_frame(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->writeFrame_w_loc, 0);
  REQUIRE_LOCATION(symaddrs->writeFrame_ctx_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->http2serverConn_hpackEncoder_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2serverConn_conn_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2writeResHeaders_streamID_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2writeResHeaders_endStream_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* http2writeResHeaders_ptr = NULL;
  assign_arg(&http2writeResHeaders_ptr, sizeof(http2writeResHeaders_ptr),
             symaddrs->writeFrame_w_loc, sp, regs);

  struct go_interface http2writeContext = {};
  assign_arg(&http2writeContext, sizeof(http2writeContext), symaddrs->writeFrame_ctx_loc, sp, regs);

  void* http2serverConn_ptr = http2writeContext.ptr;

  // ------------------------------------------------------
  // Extract members of http2writeResHeaders_ptr (stream_id, end_stream)
  // ------------------------------------------------------

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t),
                 http2writeResHeaders_ptr + symaddrs->http2writeResHeaders_streamID_offset);

  bool end_stream;
  bpf_probe_read(&end_stream, sizeof(bool),
                 http2writeResHeaders_ptr + symaddrs->http2writeResHeaders_endStream_offset);

  // ------------------------------------------------------
  // Extract members of http2serverConn_ptr (encoder, fd)
  // ------------------------------------------------------

  void* henc_addr;
  bpf_probe_read(&henc_addr, sizeof(void*),
                 http2serverConn_ptr + symaddrs->http2serverConn_hpackEncoder_offset);

  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf),
                 http2serverConn_ptr + symaddrs->http2serverConn_conn_offset);

  const int32_t fd = get_fd_from_conn_intf(conn_intf);
  if (fd == kInvalidFD) {
    return 0;
  }

  // ------------------------------------------------------
  // Prepare to submit headers to perf buffer
  // ------------------------------------------------------

  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return 0;
  }

  struct go_grpc_event_attr_t attr = {};
  attr.conn_id = conn_info->conn_id;
  attr.stream_id = stream_id;

  // We don't have the header values yet, and they are not easy to get from this probe,
  // so we just stash the information collected so far.
  // A separate probe, on the hpack encoder monitors the headers being encoded,
  // and joins that information with the stashed information collected here.
  // The key is the encoder instance.
  active_write_headers_frame_map.update(&henc_addr, &attr);

  // TODO(oazizi): Content beyond this point needs to move to return probe of the same function.

  if (end_stream) {
    struct go_grpc_http2_header_event_t* event = get_header_event();
    if (event == NULL) {
      return 0;
    }

    event->attr.probe_type = k_probe_http_http2writeResHeaders_write_frame;
    event->attr.event_type = kHeaderEventWrite;
    event->attr.timestamp_ns = bpf_ktime_get_ns();
    event->attr.conn_id = conn_info->conn_id;
    event->attr.stream_id = stream_id;
    event->name.size = 0;
    event->value.size = 0;
    event->attr.end_stream = true;

    go_grpc_events.perf_submit(ctx, event, sizeof(*event));
  }

  // TODO(oazizi): We are leaking BPF map entries until this line is activated,
  // which can only happen once we have return probes enabled.
  // active_write_headers_frame_map.update(&henc_addr, &attr);

  return 0;
}

//-----------------------------------------------------------------------------
// HTTP2 Data Tracing Functions
//-----------------------------------------------------------------------------

static __inline void go_http2_submit_data(struct pt_regs* ctx, enum http2_probe_type_t probe_type,
                                          uint32_t tgid, int32_t fd, enum grpc_event_type_t type,
                                          uint32_t stream_id, bool end_stream, char* data_ptr,
                                          int64_t data_len) {
  struct conn_info_t* conn_info = get_or_create_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }

  struct go_grpc_data_event_t* info = get_data_event();
  if (info == NULL) {
    return;
  }

  info->attr.conn_id = conn_info->conn_id;
  info->attr.timestamp_ns = bpf_ktime_get_ns();
  info->attr.probe_type = probe_type;
  info->attr.event_type = type;
  info->attr.stream_id = stream_id;
  info->attr.end_stream = end_stream;

  if (type == kDataFrameEventWrite) {
    info->data_attr.pos = conn_info->app_wr_bytes;
    conn_info->app_wr_bytes += data_len;
  } else if (type == kDataFrameEventRead) {
    info->data_attr.pos = conn_info->app_rd_bytes;
    conn_info->app_rd_bytes += data_len;
  }

  info->data_attr.data_size = data_len;

  uint32_t data_buf_size = min_uint32_t(data_len, MAX_DATA_SIZE);
  info->data_attr.data_buf_size = data_buf_size;

  // Note that we have some black magic below with the string sizes.
  // This is to avoid passing a size of 0 to bpf_probe_read(),
  // which causes BPF verifier issues on kernel 4.14.
  // The black magic includes an asm volatile, because otherwise Clang
  // will optimize our magic away.
  size_t data_buf_size_minus_1 = data_buf_size - 1;
  asm volatile("" : "+r"(data_buf_size_minus_1) :);
  data_buf_size = data_buf_size_minus_1 + 1;

  if (data_buf_size_minus_1 < MAX_DATA_SIZE) {
    bpf_probe_read(info->data, data_buf_size, data_ptr);
    go_grpc_events.perf_submit(ctx, info,
                               sizeof(info->attr) + sizeof(info->data_attr) + data_buf_size);
  }
}

// Probes golang.org/x/net/http2.Framer for payload.
//
// As a proxy for the return probe on ReadFrame(), we currently probe checkFrameOrder,
// since return probes don't work for Go.
//
// Function signature:
//   func (fr *Framer) checkFrameOrder(f Frame) error
//
// Symbol:
//   golang.org/x/net/http2.(*Framer).checkFrameOrder
//
// Verified to be stable from at least go1.6 to t go.1.13.
int probe_http2_framer_check_frame_order(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->http2_checkFrameOrder_fr_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2_checkFrameOrder_f_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->FrameHeader_Type_offset, 0);
  REQUIRE_SYMADDR(symaddrs->FrameHeader_Flags_offset, 0);
  REQUIRE_SYMADDR(symaddrs->FrameHeader_StreamID_offset, 0);
  REQUIRE_SYMADDR(symaddrs->DataFrame_data_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* framer_ptr = NULL;
  assign_arg(&framer_ptr, sizeof(framer_ptr), symaddrs->http2_checkFrameOrder_fr_loc, sp, regs);

  struct go_interface frame_interface = {};
  assign_arg(&frame_interface, sizeof(frame_interface), symaddrs->http2_checkFrameOrder_f_loc, sp,
             regs);

  // ------------------------------------------------------
  // Extract members of Framer (fd)
  // ------------------------------------------------------

  int32_t fd = get_fd_from_http2_Framer(framer_ptr, symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  // ------------------------------------------------------
  // Extract members of FrameHeader (type, flags, stream_id)
  // ------------------------------------------------------

  // All Frame types start with a frame header, so this is safe.
  // TODO(oazizi): Is there a more robust way based on DWARF info.
  // This would be required for dynamic tracing anyways.
  void* frame_header_ptr = frame_interface.ptr;

  uint8_t frame_type;
  bpf_probe_read(&frame_type, sizeof(uint8_t),
                 frame_header_ptr + symaddrs->FrameHeader_Type_offset);

  uint8_t flags;
  bpf_probe_read(&flags, sizeof(uint8_t), frame_header_ptr + symaddrs->FrameHeader_Flags_offset);
  const bool end_stream = flags & kFlagDataEndStream;

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t),
                 frame_header_ptr + symaddrs->FrameHeader_StreamID_offset);

  // Consider only data frames (0).
  if (frame_type != 0) {
    return 0;
  }

  // ------------------------------------------------------
  // Extract members of DataFrame (data)
  // ------------------------------------------------------

  // Reinterpret as data frame.
  void* data_frame_ptr = frame_interface.ptr;

  char* data_ptr;
  bpf_probe_read(&data_ptr, sizeof(char*),
                 data_frame_ptr + symaddrs->DataFrame_data_offset + kGoArrayPtrOffset);

  int64_t data_len;
  bpf_probe_read(&data_len, sizeof(int64_t),
                 data_frame_ptr + symaddrs->DataFrame_data_offset + kGoArrayLenOffset);

  // ------------------------------------------------------
  // Submit
  // ------------------------------------------------------

  go_http2_submit_data(ctx, k_probe_http2_framer_check_frame_order, tgid, fd, kDataFrameEventRead,
                       stream_id, end_stream, data_ptr, data_len);
  return 0;
}

// Probes net/http.http2Framer for HTTP2 payload.
//
// As a proxy for the return probe on ReadFrame(), we currently probe checkFrameOrder,
// since return probes don't work for Go.
//
// Function signature:
//   func (fr *http2Framer) checkFrameOrder(f http2Frame) error
//
// Symbol:
//   net/http.(*http2Framer).checkFrameOrder
//
// Verified to be stable from at least go1.?? to go.1.13.
int probe_http_http2framer_check_frame_order(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->http2Framer_checkFrameOrder_fr_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Framer_checkFrameOrder_f_loc, 0);

  // Required member offsets.
  REQUIRE_SYMADDR(symaddrs->http2FrameHeader_Type_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2FrameHeader_Flags_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2FrameHeader_StreamID_offset, 0);
  REQUIRE_SYMADDR(symaddrs->http2DataFrame_data_offset, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* framer_ptr = NULL;
  assign_arg(&framer_ptr, sizeof(framer_ptr), symaddrs->http2Framer_checkFrameOrder_fr_loc, sp,
             regs);

  struct go_interface frame_interface = {};
  assign_arg(&frame_interface, sizeof(frame_interface), symaddrs->http2Framer_checkFrameOrder_f_loc,
             sp, regs);

  // ------------------------------------------------------
  // Extract members of Framer (fd)
  // ------------------------------------------------------

  int32_t fd = get_fd_from_http_http2Framer(framer_ptr, symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  // ------------------------------------------------------
  // Extract members of http2FrameHeader (type, flags, stream_id)
  // ------------------------------------------------------

  // All Frame types start with a frame header, so this is safe.
  // TODO(oazizi): Is there a more robust way based on DWARF info.
  // This would be required for dynamic tracing anyways.
  void* frame_header_ptr = frame_interface.ptr;

  uint8_t frame_type;
  bpf_probe_read(&frame_type, sizeof(uint8_t),
                 frame_header_ptr + symaddrs->http2FrameHeader_Type_offset);

  uint8_t flags;
  bpf_probe_read(&flags, sizeof(uint8_t),
                 frame_header_ptr + symaddrs->http2FrameHeader_Flags_offset);
  const bool end_stream = flags & kFlagDataEndStream;

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t),
                 frame_header_ptr + symaddrs->http2FrameHeader_StreamID_offset);

  // Consider only data frames (0).
  if (frame_type != 0) {
    return 0;
  }

  // ------------------------------------------------------
  // Extract members of DataFrame (data)
  // ------------------------------------------------------

  // Reinterpret as data frame.
  void* data_frame_ptr = frame_interface.ptr;

  char* data_ptr;
  bpf_probe_read(&data_ptr, sizeof(char*),
                 data_frame_ptr + symaddrs->http2DataFrame_data_offset + kGoArrayPtrOffset);

  int64_t data_len;
  bpf_probe_read(&data_len, sizeof(int64_t),
                 data_frame_ptr + symaddrs->http2DataFrame_data_offset + kGoArrayLenOffset);

  // ------------------------------------------------------
  // Submit
  // ------------------------------------------------------

  go_http2_submit_data(ctx, k_probe_http_http2Framer_check_frame_order, tgid, fd,
                       kDataFrameEventRead, stream_id, end_stream, data_ptr, data_len);
  return 0;
}

// Probe for the golang.org/x/net/http2 library's frame writer.
//
// Function signature:
//   func (f *Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte) error
//
// Symbol:
//   golang.org/x/net/http2.(*Framer).WriteDataPadded
//
// Verified to be stable from go1.7 to t go.1.13.
int probe_http2_framer_write_data(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->http2_WriteDataPadded_f_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2_WriteDataPadded_streamID_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2_WriteDataPadded_endStream_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2_WriteDataPadded_data_ptr_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2_WriteDataPadded_data_len_loc, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* framer_ptr = NULL;
  assign_arg(&framer_ptr, sizeof(framer_ptr), symaddrs->http2_WriteDataPadded_f_loc, sp, regs);

  uint32_t stream_id = 0;
  assign_arg(&stream_id, sizeof(stream_id), symaddrs->http2_WriteDataPadded_streamID_loc, sp, regs);

  bool end_stream = 0;
  assign_arg(&end_stream, sizeof(end_stream), symaddrs->http2_WriteDataPadded_endStream_loc, sp,
             regs);

  char* data_ptr = NULL;
  assign_arg(&data_ptr, sizeof(data_ptr), symaddrs->http2_WriteDataPadded_data_ptr_loc, sp, regs);

  int64_t data_len = 0;
  assign_arg(&data_len, sizeof(data_len), symaddrs->http2_WriteDataPadded_data_len_loc, sp, regs);

  // ------------------------------------------------------
  // Extract members of Framer (fd)
  // ------------------------------------------------------

  int32_t fd = get_fd_from_http2_Framer(framer_ptr, symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  // ---------------------------------------------
  // Submit
  // ---------------------------------------------

  go_http2_submit_data(ctx, k_probe_http2_framer_write_data, tgid, fd, kDataFrameEventWrite,
                       stream_id, end_stream, data_ptr, data_len);

  return 0;
}

// Probe for the net/http library's frame writer.
//
// Function signature:
//   func (f *http2Framer) WriteDataPadded(streamID uint32, endStream bool, data, pad []byte) error
//
// Symbol:
//   net/http.(*http2Framer).WriteDataPadded
//
// Verified to be stable from go1.?? to t go.1.13.
int probe_http_http2framer_write_data(struct pt_regs* ctx) {
  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct go_http2_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return 0;
  }

  // Required argument offsets.
  REQUIRE_LOCATION(symaddrs->http2Framer_WriteDataPadded_f_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Framer_WriteDataPadded_streamID_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Framer_WriteDataPadded_endStream_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Framer_WriteDataPadded_data_ptr_loc, 0);
  REQUIRE_LOCATION(symaddrs->http2Framer_WriteDataPadded_data_len_loc, 0);

  // ---------------------------------------------
  // Extract arguments
  // ---------------------------------------------

  const void* sp = (const void*)ctx->sp;
  uint64_t* regs = go_regabi_regs(ctx);
  if (regs == NULL) {
    return 0;
  }

  void* framer_ptr = NULL;
  assign_arg(&framer_ptr, sizeof(framer_ptr), symaddrs->http2Framer_WriteDataPadded_f_loc, sp,
             regs);

  uint32_t stream_id = 0;
  assign_arg(&stream_id, sizeof(stream_id), symaddrs->http2Framer_WriteDataPadded_streamID_loc, sp,
             regs);

  bool end_stream = 0;
  assign_arg(&end_stream, sizeof(end_stream), symaddrs->http2Framer_WriteDataPadded_endStream_loc,
             sp, regs);

  char* data_ptr = NULL;
  assign_arg(&data_ptr, sizeof(data_ptr), symaddrs->http2Framer_WriteDataPadded_data_ptr_loc, sp,
             regs);

  int64_t data_len = 0;
  assign_arg(&data_len, sizeof(data_len), symaddrs->http2Framer_WriteDataPadded_data_len_loc, sp,
             regs);

  // ------------------------------------------------------
  // Extract members of Framer (fd)
  // ------------------------------------------------------

  int32_t fd = get_fd_from_http_http2Framer(framer_ptr, symaddrs);
  if (fd == kInvalidFD) {
    return 0;
  }

  // ---------------------------------------------
  // Submit
  // ---------------------------------------------

  go_http2_submit_data(ctx, k_probe_http_http2Framer_write_data, tgid, fd, kDataFrameEventWrite,
                       stream_id, end_stream, data_ptr, data_len);
  return 0;
}
