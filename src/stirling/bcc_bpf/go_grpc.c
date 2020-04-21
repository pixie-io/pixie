// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include "src/stirling/bcc_bpf_interface/go_grpc_types.h"

#define HEADER_COUNT 64

BPF_PERF_OUTPUT(go_grpc_header_events);
BPF_PERF_OUTPUT(go_grpc_data_events);

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(data_event_buffer_heap, struct go_grpc_data_event_t, 1);
static __inline struct go_grpc_data_event_t* get_data_event() {
  uint32_t kZero = 0;
  return data_event_buffer_heap.lookup(&kZero);
}

// Key: TGID
// Value: Symbol addresses for the binary with that TGID.
BPF_HASH(http2_symaddrs_map, uint32_t, struct conn_symaddrs_t);

// This map is used to help extract HTTP2 headers from the net/http library.
// The tracing process requires multiple probes:
//  - The primary probe collects context and sets this map entry.
//  - Dependent probes trace functions called by the primary function;
//    these read the map to get the context.
//  - The return probe of the primary function deletes the map entry.
//
// Key: encoder instance pointer
// Value: Header attributes (e.g. stream_id, fd)
BPF_HASH(active_write_headers_frame_map, void*, struct header_attr_t);

// From golang source:
// //A HeaderField is a name-value pair. Both the name and value are
// // treated as opaque sequences of octets.
// type HeaderField struct {
//   Name, Value string
//
//   // Sensitive means that this header field should never be
//   // indexed.
//   Sensitive bool
// }
struct HPackHeaderField {
  struct gostring name;
  struct gostring value;
  bool sensitive;
};

// From golang source:
// type FD struct {
//   fdmu fdMutex
//   // fdMutex is 16 bytes.
//   type fdMutex struct {
//     state uint64
//     rsema uint32
//     wsema uint32
//   }
//   Sysfd int
//   ...
// }
struct FD {
  uint64_t fdmu0;
  uint64_t fdmu1;
  int64_t sysfd;
};

// C Implementation from FrameHeader struct from golang source code:
// type FrameHeader struct {
//   valid bool  // 1 byte
//   Type FrameType  // 1 byte
//   Flags Flags  // 1 byte, a bit mask
//   Length uint32  // 4 bytes
//   StreamID uint32  // 4 bytes
//}
struct FrameHeader {
  bool valid;
  uint8_t frame_type;
  uint8_t flags;
  uint32_t length;
  uint32_t stream_id;
};

// Meaning of flag bits in FrameHeader flags.
// https://github.com/golang/net/blob/master/http2/frame.go
const uint8_t kFlagDataEndStream = 0x1;
const uint8_t kFlagHeadersEndStream = 0x1;

// C Implementation of the DataFrame struct from golang source:
// type DataFrame struct {
//   FrameHeader
//   data []byte
// }
struct DataFrame {
  struct FrameHeader header;
  struct go_byte_array data;
};

// There are two similar, but different framers
enum FramerType {
  // http2.Framer object
  khttp2_Framer,
  // http.http2Framer object
  khttp_http2Framer
};

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
static __inline int32_t get_fd_from_conn_intf(struct go_interface conn_intf) {
  uint64_t current_pid_tgid = bpf_get_current_pid_tgid();
  uint32_t tgid = current_pid_tgid >> 32;
  struct conn_symaddrs_t* symaddrs = http2_symaddrs_map.lookup(&tgid);
  if (symaddrs == NULL) {
    return -1;
  }

  if (conn_intf.type == symaddrs->syscall_conn) {
    const int kSyscallConnConnOffset = 0;
    bpf_probe_read(&conn_intf, sizeof(conn_intf), conn_intf.ptr + kSyscallConnConnOffset);
  }

  if (conn_intf.type == symaddrs->tls_conn) {
    const int kTLSConnConnOffset = 0;
    bpf_probe_read(&conn_intf, sizeof(conn_intf), conn_intf.ptr + kTLSConnConnOffset);
  }

  if (conn_intf.type != symaddrs->tcp_conn) {
    return -1;
  }

  void* fd_ptr;
  bpf_probe_read(&fd_ptr, sizeof(fd_ptr), conn_intf.ptr);

  struct FD fd;
  const int kFDOffset = 0;
  bpf_probe_read(&fd, sizeof(fd), fd_ptr + kFDOffset);

  return fd.sysfd;
}

// Returns the file descriptor from a http2.Framer object.
static __inline int32_t get_fd_from_http2_Framer(const void* framer_ptr) {
  // From llvm-dwarfdump -n net/http2.Framer -c ./server
  //
  // ...
  // 0x00070b39: DW_TAG_member
  //              DW_AT_name  ("w")
  //              DW_AT_data_member_location  (112)
  //              DW_AT_type  (0x000000000004a883 "io.Writer")
  //              DW_AT_unknown_2903  (0x00)
  // ...
  const int kFramerIOWriterOffset = 112;
  struct go_interface io_writer_interface;
  bpf_probe_read(&io_writer_interface, sizeof(io_writer_interface),
                 framer_ptr + kFramerIOWriterOffset);

  // At this point, we have the following struct:
  // go.itab.*google.golang.org/grpc/internal/transport.bufWriter,io.Writer
  //
  // type bufWriter struct {
  //   buf       []byte
  //   offset    int
  //   batchSize int
  //   conn      net.Conn
  //   err       error
  //
  //   onFlush func()
  // }

  const int kIOWriterConnOffset = 40;
  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf), io_writer_interface.ptr + kIOWriterConnOffset);

  return get_fd_from_conn_intf(conn_intf);
}

// Returns the file descriptor from a http.http2Framer object.
static __inline int32_t get_fd_from_http_http2Framer(const void* framer_ptr) {
  // From llvm-dwarfdump -n net/http.http2Framer -c ./server
  //
  // ...
  // 0x00070b39: DW_TAG_member
  //              DW_AT_name  ("w")
  //              DW_AT_data_member_location  (112)
  //              DW_AT_type  (0x000000000004a883 "io.Writer")
  //              DW_AT_unknown_2903  (0x00)
  // ...
  const int kFramerIOWriterOffset = 112;
  struct go_interface io_writer_interface;
  bpf_probe_read(&io_writer_interface, sizeof(io_writer_interface),
                 framer_ptr + kFramerIOWriterOffset);

  // At this point, we have the following struct:
  // go.itab.*net/http.http2bufferedWriter,io.Writer
  //
  // Where http2bufferedWriter is defined as follows:
  // type http2bufferedWriter struct {
  //   w  io.Writer     // immutable
  //   bw *bufio.Writer // non-nil when data is buffered
  // }

  // We have to dereference one more time, to get the inner io.Writer:
  const int kHTTP2BufferedWriterIOWriterOffset = 0;
  struct go_interface inner_io_writer_interface;
  bpf_probe_read(&inner_io_writer_interface, sizeof(inner_io_writer_interface),
                 io_writer_interface.ptr + kHTTP2BufferedWriterIOWriterOffset);

  // Now get the struct implementing net.Conn.
  const int kIOWriterConnOffset = 0;
  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf),
                 inner_io_writer_interface.ptr + kIOWriterConnOffset);

  return get_fd_from_conn_intf(conn_intf);
}

static __inline int32_t get_fd_from_framer(enum FramerType framer_type, const void* framer_ptr) {
  switch (framer_type) {
    case khttp2_Framer:
      return get_fd_from_http2_Framer(framer_ptr);
    case khttp_http2Framer:
      return get_fd_from_http_http2Framer(framer_ptr);
    default:
      return -1;
  }
}

//-----------------------------------------------------------------------------
// HTTP2 Header Tracing Functions
//-----------------------------------------------------------------------------

// TODO(oazizi): Convert to use designated initializers.
// Example:
// Instead of
//   x = {};
//   x.foo = 5;
// Use:
//   x = { .foo = 5 }

static __inline void fill_header_field(struct go_grpc_http2_header_event_t* event,
                                       const struct HPackHeaderField* user_space_ptr) {
  struct HPackHeaderField field;
  bpf_probe_read(&field, sizeof(struct HPackHeaderField), user_space_ptr);

  // Note that we read one extra byte for name and value.
  // This is to avoid passing a size of 0 to bpf_probe_read(),
  // which causes BPF verifier issues on kernel 4.14.

  event->name.size = BPF_LEN_CAP(field.name.len, HEADER_FIELD_STR_SIZE);
  bpf_probe_read(event->name.msg, event->name.size + 1, field.name.ptr);

  event->value.size = BPF_LEN_CAP(field.value.len, HEADER_FIELD_STR_SIZE);
  bpf_probe_read(event->value.msg, event->value.size + 1, field.value.ptr);
}

static __inline void submit_headers(struct pt_regs* ctx, enum HeaderEventType type, int32_t fd,
                                    uint32_t stream_id, bool end_stream,
                                    struct go_ptr_array fields) {
  uint64_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct conn_info_t* conn_info = get_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }
  conn_info->addr_valid = true;

  struct go_grpc_http2_header_event_t event = {};
  event.attr.type = type;
  event.attr.timestamp_ns = bpf_ktime_get_ns();
  event.attr.conn_id = conn_info->conn_id;
  event.attr.stream_id = stream_id;

  const struct HPackHeaderField* fields_ptr = fields.ptr;
#pragma unroll
  for (unsigned int i = 0; i < HEADER_COUNT && i < fields.len; ++i) {
    fill_header_field(&event, fields_ptr + i);
    go_grpc_header_events.perf_submit(ctx, &event, sizeof(event));
  }

  // If end of stream, send one extra empty header with end-stream flag set.
  if (end_stream) {
    event.name.size = 0;
    event.value.size = 0;
    event.attr.end_stream = true;
    go_grpc_header_events.perf_submit(ctx, &event, sizeof(event));
  }
}

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
  // The following code tries to extract the file descriptor held by the loopyWriter object, when
  // probing the (*loopyWriter).writeHeader() function call. The whole structure looks like:
  // type loopyWriter struct {
  //   ...  // 40 bytes offset
  //   framer *framer
  //   type framer struct {
  //     writer *bufWriter
  //     type bufWriter struct {
  //       ...  // 40 bytes offset
  //       conn net.Conn
  //       type net.Conn interface {
  //         ...  // 8 bytes offset
  //         data  // An pointer to *net.TCPConn, which implements the net.Conn interface.
  //         type TCPConn struct {
  //           conn  // conn is embedded inside TCPConn, which is defined as follows.
  //           type conn struct {
  //             fd *netFD
  //             type netFD struct {
  //               pfd poll.FD
  //               type FD struct {
  //                 ...  // 16 bytes offset
  //                 Sysfd int
  //               }
  //             }
  //           }
  //         }
  //       }
  //     }
  //   }
  // }
  const void* sp = (const void*)PT_REGS_SP(ctx);
  const int kLoopyWriterParamOffset = 8;
  const void* loopy_writer_ptr = *(const void**)(sp + kLoopyWriterParamOffset);

  const int kStreamIDParamOffset = 16;
  uint32_t stream_id = *(uint32_t*)(sp + kStreamIDParamOffset);

  const int kEndStreamParamOffset = 20;
  bool end_stream = *(bool*)(sp + kEndStreamParamOffset);

  const int kHeaderFieldSliceParamOffset = 24;
  const struct go_ptr_array fields =
      *(const struct go_ptr_array*)(sp + kHeaderFieldSliceParamOffset);

  const int kFramerFieldOffset = 40;
  const void* framer_ptr = *(const void**)(loopy_writer_ptr + kFramerFieldOffset);
  struct go_grpc_framer_t go_grpc_framer;
  bpf_probe_read(&go_grpc_framer, sizeof(go_grpc_framer), framer_ptr);
  const int32_t fd = get_fd_from_http2_Framer(go_grpc_framer.http2_framer);
  if (fd < 0) {
    return 0;
  }

  submit_headers(ctx, kHeaderEventWrite, fd, stream_id, end_stream, fields);

  return 0;
}

// Shared helper function for:
//   probe_http2_client_operate_headers()
//   probe_http2_server_operate_headers()
// The two probes are similar but the conn_intf location is specific to each struct.
static __inline void probe_http2_operate_headers(struct pt_regs* ctx, struct go_interface conn_intf,
                                                 const void* frame_ptr) {
  const void* frame_header_ptr = *(const void**)frame_ptr;

  // type FrameHeader {
  //   valid bool      // 1 byte
  //   Type FrameType  // 1 byte
  //   Flags Flags     // We are looking for this field.
  // }
  const int kFlagsOffset = 2;
  const uint8_t flags = *(const uint8_t*)(frame_header_ptr + kFlagsOffset);
  const bool end_stream = flags & kFlagHeadersEndStream;

  const int kStreamIDOffset = 8;
  const uint32_t stream_id = *(const uint32_t*)(frame_header_ptr + kStreamIDOffset);

  const int kFieldsOffset = 8;
  const struct go_ptr_array fields = *(const struct go_ptr_array*)(frame_ptr + kFieldsOffset);
  const struct HPackHeaderField* fields_ptr = fields.ptr;

  // TODO(yzhao): We saw some arbitrary large slices received by operateHeaders(), it's not clear
  // what conditions result into them.
  if (fields.len > 100 || fields.len <= 0 || fields.cap <= 0) {
    return;
  }

  uint64_t tgid = bpf_get_current_pid_tgid() >> 32;

  const uint32_t fd = get_fd_from_conn_intf(conn_intf);
  if (fd < 0) {
    return;
  }

  submit_headers(ctx, kHeaderEventRead, fd, stream_id, end_stream, fields);
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
  const void* sp = (const void*)PT_REGS_SP(ctx);

  const int kHTTP2ClientParamOffset = 8;
  const void* http2_client_ptr = *(const void**)(sp + kHTTP2ClientParamOffset);

  const int kFrameParamOffset = 16;
  const void* frame_ptr = *(const void**)(sp + kFrameParamOffset);

  struct go_interface conn_intf;
  const int kHTTP2ClientConnFieldOffset = 64;
  bpf_probe_read(&conn_intf, sizeof(conn_intf), http2_client_ptr + kHTTP2ClientConnFieldOffset);

  probe_http2_operate_headers(ctx, conn_intf, frame_ptr);

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
  const void* sp = (const void*)PT_REGS_SP(ctx);

  const int kHTTP2ServerParamOffset = 8;
  const void* http2_server_ptr = *(const void**)(sp + kHTTP2ServerParamOffset);

  const int kFrameParamOffset = 16;
  const void* frame_ptr = *(const void**)(sp + kFrameParamOffset);

  struct go_interface conn_intf;
  const int kHTTP2ServerConnFieldOffset = 32;
  bpf_probe_read(&conn_intf, sizeof(conn_intf), http2_server_ptr + kHTTP2ServerConnFieldOffset);

  probe_http2_operate_headers(ctx, conn_intf, frame_ptr);

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
  const void* sp = (const void*)PT_REGS_SP(ctx);

  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  // Receiver is (*http2serverConn).
  const int kReceiverOffset = 8;
  void* http2serverConn_ptr;
  bpf_probe_read(&http2serverConn_ptr, sizeof(void*), sp + kReceiverOffset);

  // Param 1 is (*http2MetaHeadersFrame).
  const int kParam1Offset = 16;
  void* http2MetaHeadersFrame_ptr;
  bpf_probe_read(&http2MetaHeadersFrame_ptr, sizeof(void*), sp + kParam1Offset);

  // ------------------------------------------------------
  // Extract members of http2MetaHeadersFrame_ptr (headers)
  // ------------------------------------------------------

  const int kFrameFieldsOffset = 8;
  struct go_ptr_array fields;
  bpf_probe_read(&fields, sizeof(struct go_ptr_array),
                 http2MetaHeadersFrame_ptr + kFrameFieldsOffset);

  const int kMetaHeadersFrameHeadersFrameOffset = 0;
  void* http2HeadersFrame_ptr;
  bpf_probe_read(&http2HeadersFrame_ptr, sizeof(void*),
                 http2MetaHeadersFrame_ptr + kMetaHeadersFrameHeadersFrameOffset);

  // ------------------------------------------------------
  // Extract members of http2HeadersFrame_ptr (stream_id, end_stream)
  // ------------------------------------------------------

  const int kFrameFlagsOffset = 2;
  uint8_t flags;
  bpf_probe_read(&flags, sizeof(uint8_t), http2HeadersFrame_ptr + kFrameFlagsOffset);
  const bool end_stream = flags & kFlagHeadersEndStream;

  const int kFrameStreamIDOffset = 8;
  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t), http2HeadersFrame_ptr + kFrameStreamIDOffset);

  // ------------------------------------------------------
  // Extract members of http2serverConn_ptr (fd)
  // ------------------------------------------------------

  const int kHTTP2ServerConnFieldOffset = 16;
  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf), http2serverConn_ptr + kHTTP2ServerConnFieldOffset);

  int32_t fd = get_fd_from_conn_intf(conn_intf);
  if (fd < 0) {
    return 0;
  }

  // ------------------------------------------------------
  // Wrap-ups
  // ------------------------------------------------------

  submit_headers(ctx, kHeaderEventRead, fd, stream_id, end_stream, fields);

  return 0;
}

static __inline void submit_header(struct pt_regs* ctx, enum HeaderEventType type,
                                   void* encoder_ptr, struct HPackHeaderField* header_field_ptr) {
  struct header_attr_t* attr = active_write_headers_frame_map.lookup(&encoder_ptr);
  if (attr == NULL) {
    return;
  }

  struct HPackHeaderField header_field;
  bpf_probe_read(&header_field, sizeof(header_field), header_field_ptr);

  struct go_grpc_http2_header_event_t event = {};
  event.attr.type = type;
  event.attr.timestamp_ns = bpf_ktime_get_ns();
  event.attr.conn_id = attr->conn_id;
  event.attr.stream_id = attr->stream_id;

  event.name.size = BPF_LEN_CAP(header_field.name.len, HEADER_FIELD_STR_SIZE);
  bpf_probe_read(event.name.msg, event.name.size, header_field.name.ptr);

  event.value.size = BPF_LEN_CAP(header_field.value.len, HEADER_FIELD_STR_SIZE);
  bpf_probe_read(event.value.msg, event.value.size, header_field.value.ptr);

  go_grpc_header_events.perf_submit(ctx, &event, sizeof(event));
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
  const void* sp = (const void*)ctx->sp;
  if (sp == NULL) {
    return 0;
  }

  // Receiver is (*Encoder).
  const int kReceiverOffset = 8;
  void* encoder_ptr;
  bpf_probe_read(&encoder_ptr, sizeof(void*), sp + kReceiverOffset);

  // Param 1 is (HeaderField).
  const int kParam1Offset = 16;
  struct HPackHeaderField* header_field;
  bpf_probe_read(&header_field, sizeof(struct HPackHeaderField*), sp + kParam1Offset);

  submit_header(ctx, kHeaderEventWrite, encoder_ptr, header_field);

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
  // ---------------------------------------------
  // Extract arguments (on stack)
  // ---------------------------------------------

  const void* sp = (const void*)PT_REGS_SP(ctx);

  // Receiver is (*http2writeResHeaders).
  const int kReceiverOffset = 8;
  void* http2writeResHeaders_ptr;
  bpf_probe_read(&http2writeResHeaders_ptr, sizeof(void*), sp + kReceiverOffset);

  // Param 1 is (http2writeContext).
  const int kParam1Offset = 16;
  struct go_interface http2writeContext;
  bpf_probe_read(&http2writeContext, sizeof(struct go_interface), sp + kParam1Offset);

  void* http2serverConn_ptr = http2writeContext.ptr;

  // ------------------------------------------------------
  // Extract members of http2writeResHeaders_ptr (stream_id, end_stream)
  // ------------------------------------------------------

  const int kWriteResHeadersStreamIDOffset = 0;
  const int kWriteResHeadersEndStreamOffset = 48;

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t),
                 http2writeResHeaders_ptr + kWriteResHeadersStreamIDOffset);

  bool end_stream;
  bpf_probe_read(&end_stream, sizeof(bool),
                 http2writeResHeaders_ptr + kWriteResHeadersEndStreamOffset);

  // ------------------------------------------------------
  // Extract members of http2serverConn_ptr (encoder, fd)
  // ------------------------------------------------------

  const int kWriteContextHpackEncoderOffset = 0x168;
  void* henc_addr;
  bpf_probe_read(&henc_addr, sizeof(void*), http2serverConn_ptr + kWriteContextHpackEncoderOffset);

  const int kWriteContextConnOffset = 16;
  struct go_interface conn_intf;
  bpf_probe_read(&conn_intf, sizeof(conn_intf), http2serverConn_ptr + kWriteContextConnOffset);

  int32_t fd = get_fd_from_conn_intf(conn_intf);

  // ------------------------------------------------------
  // Prepare to submit headers to perf buffer
  // ------------------------------------------------------

  uint64_t tgid = bpf_get_current_pid_tgid() >> 32;
  struct conn_info_t* conn_info = get_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return 0;
  }
  conn_info->addr_valid = true;

  struct header_attr_t attr = {};
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
    struct go_grpc_http2_header_event_t event = {};
    event.attr.type = kHeaderEventWrite;
    event.attr.timestamp_ns = bpf_ktime_get_ns();
    event.attr.conn_id = conn_info->conn_id;
    event.attr.stream_id = stream_id;
    event.name.size = 0;
    event.value.size = 0;
    event.attr.end_stream = true;
    go_grpc_header_events.perf_submit(ctx, &event, sizeof(event));
  }

  // TODO(oazizi): We are leaking BPF map entries until this line is activated,
  // which can only happen once we have return probes enabled.
  // active_write_headers_frame_map.update(&henc_addr, &attr);

  return 0;
}

//-----------------------------------------------------------------------------
// HTTP2 Data Tracing Functions
//-----------------------------------------------------------------------------

// Shared helper function for:
//   probe_http2_framer_write_data()
//   probe_http_http2framer_write_data()
static __inline void probe_write_data(struct pt_regs* ctx, enum FramerType framer_type) {
  const char* sp = (const char*)ctx->sp;
  if (sp == NULL) {
    return;
  }

  // Param 0 (receiver) is (fr *Framer).
  const int kParam0Offset = 8;

  // Param 1 is (streamID uint32).
  const int kParam1Offset = 16;

  // Param 2 is (endStream bool).
  const int kParam2Offset = 20;

  // Param 3 is (data []byte).
  const int kParam3Offset = 24;

  void* framer_ptr;
  bpf_probe_read(&framer_ptr, sizeof(void*), sp + kParam0Offset);

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t), sp + kParam1Offset);

  bool end_stream;
  bpf_probe_read(&end_stream, sizeof(bool), sp + kParam2Offset);

  struct go_byte_array data;
  bpf_probe_read(&data, sizeof(struct go_byte_array), sp + kParam3Offset);

  struct go_grpc_data_event_t* info = get_data_event();
  if (info == NULL) {
    return;
  }

  uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
  int32_t fd = get_fd_from_framer(framer_type, framer_ptr);
  if (fd < 0) {
    return;
  }

  struct conn_info_t* conn_info = get_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }

  info->attr.type = kDataFrameEventWrite;
  info->attr.timestamp_ns = bpf_ktime_get_ns();
  info->attr.conn_id = conn_info->conn_id;
  info->attr.stream_id = stream_id;
  info->attr.end_stream = end_stream;
  uint32_t data_len = BPF_LEN_CAP(data.len, MAX_DATA_SIZE);
  info->attr.data_len = data_len;
  bpf_probe_read(info->data, data_len + 1, data.ptr);

  // Replacing data_len with info->attr.data_len causes BPF verifier to reject the statement below.
  // Possibly because it lost track of the value because of the indirect access to info->attr.
  // TODO(yzhao): This could be one reason to prefer flat data structure to grouped attributes,
  // as its simplicity is more friendly to BPF verifier.
  go_grpc_data_events.perf_submit(ctx, info, sizeof(info->attr) + data_len);
}

// Shared helper function for:
//   probe_http2_framer_check_frame_order()
//   probe_http_http2framer_check_frame_order()
static __inline void probe_check_frame_order(struct pt_regs* ctx, enum FramerType framer_type) {
  const char* sp = (const char*)ctx->sp;
  if (sp == NULL) {
    return;
  }

  // Param 0 (receiver) is (*Framer) or (*http2Framer).
  const int kParam0Offset = 8;

  // Param 1 is (Frame) or (http2Frame).
  const int kParam1Offset = 16;

  // NOTE: We get lucky enough that http.http2Frame interface is similar enough to http2.Frame,
  // that we don't currently have to differentiate the two.
  struct go_interface frame_interface;
  bpf_probe_read(&frame_interface, sizeof(struct go_interface), sp + kParam1Offset);

  // All frames types start with a frame header, so this is safe.
  struct FrameHeader* frame_header_ptr;
  bpf_probe_read(&frame_header_ptr, sizeof(struct FrameHeader*), &frame_interface.ptr);

  uint8_t flags;
  bpf_probe_read(&flags, sizeof(uint8_t), &frame_header_ptr->flags);
  const bool end_stream = flags & kFlagDataEndStream;

  uint8_t frame_type;
  bpf_probe_read(&frame_type, sizeof(uint8_t), &frame_header_ptr->frame_type);

  uint32_t stream_id;
  bpf_probe_read(&stream_id, sizeof(uint32_t), &frame_header_ptr->stream_id);

  // Consider only data frames (0).
  if (frame_type == 0) {
    void* framer_ptr;
    bpf_probe_read(&framer_ptr, sizeof(void*), sp + kParam0Offset);

    // Reinterpret as data frame.
    struct DataFrame* data_frame_ptr = (struct DataFrame*)frame_header_ptr;
    struct go_byte_array data;
    bpf_probe_read(&data, sizeof(struct go_byte_array), &data_frame_ptr->data);

    struct go_grpc_data_event_t* info = get_data_event();
    if (info == NULL) {
      return;
    }

    uint32_t tgid = bpf_get_current_pid_tgid() >> 32;
    int32_t fd = get_fd_from_framer(framer_type, framer_ptr);
    if (fd < 0) {
      return;
    }

    struct conn_info_t* conn_info = get_conn_info(tgid, fd);
    if (conn_info == NULL) {
      return;
    }
    info->attr.conn_id = conn_info->conn_id;

    info->attr.timestamp_ns = bpf_ktime_get_ns();
    info->attr.type = kDataFrameEventRead;
    info->attr.timestamp_ns = bpf_ktime_get_ns();
    info->attr.conn_id = conn_info->conn_id;
    info->attr.stream_id = stream_id;
    info->attr.end_stream = end_stream;
    uint32_t data_len = BPF_LEN_CAP(data.len, MAX_DATA_SIZE);
    info->attr.data_len = data_len;
    bpf_probe_read(info->data, data_len + 1, data.ptr);

    go_grpc_data_events.perf_submit(ctx, info, sizeof(info->attr) + data_len);
  }
}

// Probe for the golang.org/x/net/http2 library's HTTP2 frame reader.
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
  probe_check_frame_order(ctx, khttp2_Framer);
  return 0;
}

// Probe for the net/http library's frame reader.
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
  probe_check_frame_order(ctx, khttp_http2Framer);
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
  probe_write_data(ctx, khttp2_Framer);
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
  probe_write_data(ctx, khttp_http2Framer);
  return 0;
}
