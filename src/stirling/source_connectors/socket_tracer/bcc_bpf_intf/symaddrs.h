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
};

struct go_http2_symaddrs_t {
  // ---- itable symbols ----

  // io.Writer interface types.
  int64_t http_http2bufferedWriter;  // "go.itab.*net/http.http2bufferedWriter,io.Writer
  int64_t transport_bufWriter;  // "google.golang.org/grpc/internal/transport.bufWriter,io.Writer

  // ---- struct member offsets ----

  // Arguments of net/http.(*http2Framer).WriteDataPadded.
  int32_t http2Framer_WriteDataPadded_f_offset;          // 8
  int32_t http2Framer_WriteDataPadded_streamID_offset;   // 16
  int32_t http2Framer_WriteDataPadded_endStream_offset;  // 20
  int32_t http2Framer_WriteDataPadded_data_offset;       // 24

  // Arguments of golang.org/x/net/http2.(*Framer).WriteDataPadded.
  int32_t http2_WriteDataPadded_f_offset;          // 8
  int32_t http2_WriteDataPadded_streamID_offset;   // 16
  int32_t http2_WriteDataPadded_endStream_offset;  // 20
  int32_t http2_WriteDataPadded_data_offset;       // 24

  // Arguments of net/http.(*http2Framer).checkFrameOrder.
  int32_t http2Framer_checkFrameOrder_fr_offset;  // 8
  int32_t http2Framer_checkFrameOrder_f_offset;   // 16

  // Arguments of golang.org/x/net/http2.(*Framer).checkFrameOrder.
  int32_t http2_checkFrameOrder_fr_offset;  // 8
  int32_t http2_checkFrameOrder_f_offset;   // 16

  // Arguments of net/http.(*http2writeResHeaders).writeFrame.
  int32_t writeFrame_w_offset;    // 8
  int32_t writeFrame_ctx_offset;  // 16

  // Arguments of golang.org/x/net/http2/hpack.(*Encoder).WriteField.
  int32_t WriteField_e_offset;  // 8
  int32_t WriteField_f_offset;  // 16

  // Arguments of net/http.(*http2serverConn).processHeaders.
  int32_t processHeaders_sc_offset;  // 8
  int32_t processHeaders_f_offset;   // 16

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Server).operateHeaders.
  int32_t http2Server_operateHeaders_t_offset;      // 8
  int32_t http2Server_operateHeaders_frame_offset;  // 16

  // Arguments of google.golang.org/grpc/internal/transport.(*http2Client).operateHeaders.
  int32_t http2Client_operateHeaders_t_offset;      // 8
  int32_t http2Client_operateHeaders_frame_offset;  // 16

  // Arguments of google.golang.org/grpc/internal/transport.(*loopyWriter).writeHeader.
  int32_t writeHeader_l_offset;          // 8
  int32_t writeHeader_streamID_offset;   // 16
  int32_t writeHeader_endStream_offset;  // 20
  int32_t writeHeader_hf_offset;         // 24

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
  // ---- struct member offsets ----

  // Arguments of crypto/tls.(*Conn).Write.
  int32_t Write_c_offset;  // 8
  int32_t Write_b_offset;  // 16

  // Arguments of crypto/tls.(*Conn).Read.
  int32_t Read_c_offset;  // 8
  int32_t Read_b_offset;  // 16
};

struct openssl_symaddrs_t {
  // Offset of rbio in struct ssl_st.
  // Struct is defined in ssl/ssl_local.h, ssl/ssl_locl.h, ssl/ssl_lcl.h, depending on the version.
  int32_t SSL_rbio_offset;  // 0x10;

  // Offset of num in struct bio_st.
  // Struct is defined in crypto/bio/bio_lcl.h, crypto/bio/bio_local.h depending on the version.
  int32_t RBIO_num_offset;  // 0x30 (openssl 1.1.1) or 0x28 (openssl 1.1.0)
};
