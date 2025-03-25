/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus

#include <string>

#include <absl/strings/str_format.h>

#endif

//-----------------------------------------------------------------------------
// Symbol address structs
//-----------------------------------------------------------------------------

// These structs hold information to enable tracing of statically linked libraries.
// In particular two types of information is recorded:
//   (1) Golang itable symbol addresses - to resolve types that underlie interfaces.
//   (2) Struct member offsets - so we can access the required struct members.
// This information is then communicated from user-space back to the kernel uprobes.

// Currently, the tracers that require these addresses/offsets are the golang HTTP2 and TLS probes.
// In the future, we may have libraries in other languages too (e.g. boringssl).

// A note about the naming convention of Golang information:
//  - Itable symbol address: represents the type in the itable (not the interface type).
//  - Member offsets: <library>_<function>_<argument>_offset
//  - library/function/argument all use golang symbol naming and case.

// Note: number values in comments represent known offsets, in case we need to fall back.
//       Eventually, they can be removed, because they are not reliable.

enum location_type_t {
  kLocationTypeInvalid = 0,
  kLocationTypeStack = 1,
  kLocationTypeRegisters = 2
};

struct location_t {
  enum location_type_t type;
  int32_t offset;
};

#ifdef __cplusplus
inline std::string ToString(const struct location_t& location) {
  return absl::Substitute("type=$0 offset=$1", magic_enum::enum_name(location.type),
                          location.offset);
}

inline std::ostream& operator<<(std::ostream& os, const struct location_t& location) {
  os << ToString(location);
  return os;
}

inline bool operator==(const struct location_t& a, const struct location_t& b) {
  return a.type == b.type && a.offset == b.offset;
}
#endif

// A set of symbols that are useful for various different uprobes.
// Currently, this includes mostly connection related items,
// which applies to any network protocol tracing (HTTP2, TLS, etc.).
struct go_common_symaddrs_t {
  // ---- itable symbols ----

  // net.Conn interface types.
  // go.itab.*google.golang.org/grpc/credentials/internal.syscallConn,net.Conn
  int64_t internal_syscallConn;
  int64_t tls_Conn;     // go.itab.*crypto/tls.Conn,net.Conn
  int64_t net_TCPConn;  // go.itab.*net.TCPConn,net.Conn

  // ---- struct member offsets ----

  // Members of internal/poll.FD.
  int32_t FD_Sysfd_offset;  // 16

  // Members of crypto/tls.Conn.
  int32_t tlsConn_conn_offset;  // 0

  // Members of google.golang.org/grpc/credentials/internal.syscallConn
  int32_t syscallConn_conn_offset;  // 0

  // Member of runtime.g.
  int32_t g_goid_offset;  // 152

  // Offset of the ptr to struct g from the address in %fsbase.
  int32_t g_addr_offset;  // -8
};

struct go_http2_symaddrs_t {
  // ---- itable symbols ----

  // io.Writer interface types.
  int64_t http_http2bufferedWriter;  // "go.itab.*net/http.http2bufferedWriter,io.Writer
  int64_t transport_bufWriter;  // "google.golang.org/grpc/internal/transport.bufWriter,io.Writer

  // ---- function argument locations ----

  // Arguments of net/http.(*http2Framer).WriteDataPadded.
  struct location_t http2Framer_WriteDataPadded_f_loc;          // 8
  struct location_t http2Framer_WriteDataPadded_streamID_loc;   // 16
  struct location_t http2Framer_WriteDataPadded_endStream_loc;  // 20
  struct location_t http2Framer_WriteDataPadded_data_ptr_loc;   // 24
  struct location_t http2Framer_WriteDataPadded_data_len_loc;   // 32

  // Arguments of golang.org/x/net/http2.(*Framer).WriteDataPadded.
  struct location_t http2_WriteDataPadded_f_loc;          // 8
  struct location_t http2_WriteDataPadded_streamID_loc;   // 16
  struct location_t http2_WriteDataPadded_endStream_loc;  // 20
  struct location_t http2_WriteDataPadded_data_ptr_loc;   // 24
  struct location_t http2_WriteDataPadded_data_len_loc;   // 32

  // Arguments of net/http.(*http2Framer).checkFrameOrder.
  struct location_t http2Framer_checkFrameOrder_fr_loc;  // 8
  struct location_t http2Framer_checkFrameOrder_f_loc;   // 16

  // Arguments of golang.org/x/net/http2.(*Framer).checkFrameOrder.
  struct location_t http2_checkFrameOrder_fr_loc;  // 8
  struct location_t http2_checkFrameOrder_f_loc;   // 16

  // Arguments of net/http.(*http2writeResHeaders).writeFrame.
  struct location_t writeFrame_w_loc;    // 8
  struct location_t writeFrame_ctx_loc;  // 16

  // Arguments of golang.org/x/net/http2/hpack.(*Encoder).WriteField.
  struct location_t WriteField_e_loc;  // 8
  // Note that the HeaderField `f` is further broken down to its name and value members.
  // This is done so we can better control the location of these members from user-space.
  // In theory, there could be an ABI that splits these two members across stack and registers.
  struct location_t WriteField_f_name_loc;   // 16
  struct location_t WriteField_f_value_loc;  // 32

  // Arguments of net/http.(*http2serverConn).processHeaders.
  struct location_t processHeaders_sc_loc;  // 8
  struct location_t processHeaders_f_loc;   // 16

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders.
  struct location_t http2Server_operateHeaders_t_loc;      // 8
  struct location_t http2Server_operateHeaders_frame_loc;  // 16

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders.
  struct location_t http2Client_operateHeaders_t_loc;      // 8
  struct location_t http2Client_operateHeaders_frame_loc;  // 16

  // Arguments of google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader.
  struct location_t writeHeader_l_loc;          // 8
  struct location_t writeHeader_streamID_loc;   // 16
  struct location_t writeHeader_endStream_loc;  // 20
  struct location_t writeHeader_hf_ptr_loc;     // 24
  struct location_t writeHeader_hf_len_loc;     // 32

  // ---- struct member offsets ----

  // Struct member offsets.
  // Naming maintains golang style: <struct>_<member>_offset
  // Note: values in comments represent known offsets, in case we need to fall back.
  //       Eventually, they should be removed, because they are not reliable.

  // Members of golang.org/x/net/http2/hpack.HeaderField.
  int32_t HeaderField_Name_offset;   // 0
  int32_t HeaderField_Value_offset;  // 16

  // Members of google.golang.org/grpc/internal/transport.http2Server.
  int32_t http2Server_conn_offset;  // 16 or 24

  // Members of google.golang.org/grpc/internal/transport.http2Client.
  int32_t http2Client_conn_offset;  // 64

  // Members of google.golang.org/grpc/internal/transport.loopyWriter.
  int32_t loopyWriter_framer_offset;  // 40

  // Members of golang.org/x/net/net/http2.Framer.
  int32_t Framer_w_offset;  // 112

  // Members of golang.org/x/net/http2.MetaHeadersFrame.
  int32_t MetaHeadersFrame_HeadersFrame_offset;  // 0
  int32_t MetaHeadersFrame_Fields_offset;        // 0

  // Members of golang.org/x/net/http2.HeadersFrame.
  int32_t HeadersFrame_FrameHeader_offset;  // 0

  // Members of golang.org/x/net/http2.FrameHeader.
  int32_t FrameHeader_Type_offset;      // 1
  int32_t FrameHeader_Flags_offset;     // 2
  int32_t FrameHeader_StreamID_offset;  // 8

  // Members of golang.org/x/net/http2.DataFrame.
  int32_t DataFrame_data_offset;  // 16

  // Members of google.golang.org/grpc/internal/transport.bufWriter.
  int32_t bufWriter_conn_offset;  // 40

  // Members of net/http.http2serverConn.
  int32_t http2serverConn_conn_offset;          // 16
  int32_t http2serverConn_hpackEncoder_offset;  // 360

  // Members of net/http.http2HeadersFrame
  int32_t http2HeadersFrame_http2FrameHeader_offset;  // 0

  // Members of net/http.http2FrameHeader.
  int32_t http2FrameHeader_Type_offset;      // 1
  int32_t http2FrameHeader_Flags_offset;     // 2
  int32_t http2FrameHeader_StreamID_offset;  // 8

  // Members of golang.org/x/net/http2.DataFrame.
  int32_t http2DataFrame_data_offset;  // 16

  // Members of net/http.http2writeResHeaders.
  int32_t http2writeResHeaders_streamID_offset;   // 0
  int32_t http2writeResHeaders_endStream_offset;  // 48

  // Members of net/http.http2MetaHeadersFrame.
  int32_t http2MetaHeadersFrame_http2HeadersFrame_offset;  // 0
  int32_t http2MetaHeadersFrame_Fields_offset;             // 8

  // Members of net/http.http2Framer.
  int32_t http2Framer_w_offset;  // 112

  // Members of net/http.http2bufferedWriter
  int32_t http2bufferedWriter_w_offset;  // 0
};

struct go_tls_symaddrs_t {
  // ---- function argument locations ----

  // Arguments of crypto/tls.(*Conn).Write.
  struct location_t Write_c_loc;        // 8
  struct location_t Write_b_loc;        // 16
  struct location_t Write_retval0_loc;  // 40
  struct location_t Write_retval1_loc;  // 48

  // Arguments of crypto/tls.(*Conn).Read.
  struct location_t Read_c_loc;        // 8
  struct location_t Read_b_loc;        // 16
  struct location_t Read_retval0_loc;  // 40
  struct location_t Read_retval1_loc;  // 48
};

struct openssl_symaddrs_t {
  // Offset of rbio in struct ssl_st.
  // Struct is defined in ssl/ssl_local.h, ssl/ssl_locl.h, ssl/ssl_lcl.h, depending on the version.
  int32_t SSL_rbio_offset;  // 0x10;

  // Offset of num in struct bio_st.
  // Struct is defined in crypto/bio/bio_lcl.h, crypto/bio/bio_local.h depending on the version.
  int32_t RBIO_num_offset;  // 0x30 (openssl 1.1.1) or 0x28 (openssl 1.1.0)
};

#define MAX_CMD_SIZE 32

struct openssl_trace_state_debug_t {
  char comm[MAX_CMD_SIZE];
  enum ssl_source_t ssl_source;
  enum traffic_protocol_t protocol;
  bool mismatched_fd;
};

// For reading file descriptor from a TLSWrap pointer.
struct node_tlswrap_symaddrs_t {
  // Offset of StreamListener base class of TLSWrap class.
  int32_t TLSWrap_StreamListener_offset;

  // Offset of the stream_ member variable of StreamListener class.
  // stream_ member variable is a StreamResource pointer, which points to a LibuvStreamWrap object
  int32_t StreamListener_stream_offset;

  // Offset of StreamResource base class of StreamBase class.
  // class StreamBase : public class StreamResource {...};
  int32_t StreamBase_StreamResource_offset;

  // Offset of StreamBase base class of LibuvStreamWrap class.
  // class LibuvStreamWrap : public class StreamBase {...};
  int32_t LibuvStreamWrap_StreamBase_offset;

  // Offset of stream_ member variable of LibuvStreamWrap class.
  int32_t LibuvStreamWrap_stream_offset;

  // Offset of io_watcher member variable (uv__io_s/uv__io_t type) of uv_stream_s struct, in
  // node/deps/uv.
  int32_t uv_stream_s_io_watcher_offset;

  // Offset of fd member variable of uv__io_s/uv__io_t
  int32_t uv__io_s_fd_offset;
};

#ifdef __cplusplus

inline std::string ToString(const struct node_tlswrap_symaddrs_t& symaddrs) {
  return absl::StrFormat(
      "TLSWrap_StreamListener_offset=%#x\n"
      "StreamListener_stream_offset=%#x\n"
      "StreamBase_StreamResource_offset=%#x\n"
      "LibuvStreamWrap_StreamBase_offset=%#x\n"
      "LibuvStreamWrap_stream_offset=%#x\n"
      "uv_stream_s_io_watcher_offset=%#x\n"
      "uv__io_s_fd_offset=%#x\n",
      symaddrs.TLSWrap_StreamListener_offset, symaddrs.StreamListener_stream_offset,
      symaddrs.StreamBase_StreamResource_offset, symaddrs.LibuvStreamWrap_StreamBase_offset,
      symaddrs.LibuvStreamWrap_stream_offset, symaddrs.uv_stream_s_io_watcher_offset,
      symaddrs.uv__io_s_fd_offset);
}

#endif
