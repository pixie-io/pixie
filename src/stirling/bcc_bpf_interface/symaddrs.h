#pragma once

//-----------------------------------------------------------------------------
// Symbol address structs
//-----------------------------------------------------------------------------

// These structs hold symbol addresses for various different uprobes.
// The structs are used to communicate these addresses from user-space back to the kernel uprobes.

// Currently, the uprobes that require symbol addresses are the golang HTTP2 probes.
// In the future we will probe golang applications for TLS as well.

// A set of symbols that are useful for various different uprobes.
// Currently, this includes mostly connection related items,
// which applies to any network protocol tracing (HTTP2, TLS, etc.).
struct go_common_symaddrs_t {
  // net.Conn interface types.
  // go.itab.*google.golang.org/grpc/credentials/internal.syscallConn,net.Conn
  int64_t internal_syscallConn;
  int64_t tls_Conn;     // go.itab.*crypto/tls.Conn,net.Conn
  int64_t net_TCPConn;  // go.itab.*net.TCPConn,net.Conn

  // Struct member offsets.
  // Naming maintains golang style: <struct>_<member>_offset
  // Note: values in comments represent known offsets, in case we need to fall back.
  //       Eventually, they should be removed, because they are not reliable.

  // Members of internal/poll.FD.
  int32_t FD_Sysfd_offset;  // 16

  // Members of crypto/tls.Conn.
  int32_t tlsConn_conn_offset;  // 0

  // Members of google.golang.org/grpc/credentials/internal.syscallConn
  int32_t syscallConn_conn_offset;  // 0
};

struct go_http2_symaddrs_t {
  // io.Writer interface types.
  int64_t http_http2bufferedWriter;  // "go.itab.*net/http.http2bufferedWriter,io.Writer
  int64_t transport_bufWriter;  // "google.golang.org/grpc/internal/transport.bufWriter,io.Writer

  // Argument offsets.
  // Naming maintains golang style <library>_<function>_<argument>_offset

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

#define REQUIRE_SYMADDR(symaddr, retval) \
  if (symaddr == -1) {                   \
    return retval;                       \
  }
