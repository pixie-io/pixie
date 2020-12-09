// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#pragma once

#include "src/stirling/bcc_bpf/utils.h"
#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/bcc_bpf_interface/grpc.h"

static __inline enum MessageType infer_http_message(const char* buf, size_t count) {
  // Smallest HTTP response is 17 characters:
  // HTTP/1.1 200 OK\r\n
  // Smallest HTTP response is 16 characters:
  // GET x HTTP/1.1\r\n
  if (count < 16) {
    return kUnknown;
  }

  if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
    return kResponse;
  }
  if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
    return kRequest;
  }
  if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
    return kRequest;
  }
  // TODO(oazizi): Should we add PUT, DELETE, HEAD, and perhaps others?

  return kUnknown;
}

// Cassandra frame:
//      0         8        16        24        32         40
//      +---------+---------+---------+---------+---------+
//      | version |  flags  |      stream       | opcode  |
//      +---------+---------+---------+---------+---------+
//      |                length                 |
//      +---------+---------+---------+---------+
//      |                                       |
//      .            ...  body ...              .
//      .                                       .
//      .                                       .
//      +----------------------------------------
static __inline enum MessageType infer_cql_message(const char* buf, size_t count) {
  static const uint8_t kError = 0x00;
  static const uint8_t kStartup = 0x01;
  static const uint8_t kReady = 0x02;
  static const uint8_t kAuthenticate = 0x03;
  static const uint8_t kOptions = 0x05;
  static const uint8_t kSupported = 0x06;
  static const uint8_t kQuery = 0x07;
  static const uint8_t kResult = 0x08;
  static const uint8_t kPrepare = 0x09;
  static const uint8_t kExecute = 0x0a;
  static const uint8_t kRegister = 0x0b;
  static const uint8_t kEvent = 0x0c;
  static const uint8_t kBatch = 0x0d;
  static const uint8_t kAuthChallenge = 0x0e;
  static const uint8_t kAuthResponse = 0x0f;
  static const uint8_t kAuthSuccess = 0x10;

  // Cassandra frames have a 9-byte header.
  if (count < 9) {
    return kUnknown;
  }

  // Version contains both version and direction.
  bool request = (buf[0] & 0x80) == 0x00;
  uint8_t version = (buf[0] & 0x7f);
  uint8_t flags = buf[1];
  uint8_t opcode = buf[4];
  int32_t length = read_big_endian_int32(&buf[5]);

  // Cassandra version should 5 or less. Also v2 and lower seem much less popular.
  // For example ScyllaDB only supports v3+.
  if (version < 3 || version > 5) {
    return kUnknown;
  }

  // Only flags 0x1, 0x2, 0x4 and 0x8 are used.
  if ((flags & 0xf0) != 0) {
    return kUnknown;
  }

  // A frame is limited to 256MB in length,
  // but we look for more common frames which should be much smaller in size.
  if (length > 10000) {
    return kUnknown;
  }

  switch (opcode) {
    case kStartup:
    case kOptions:
    case kQuery:
    case kPrepare:
    case kExecute:
    case kRegister:
    case kBatch:
    case kAuthResponse:
      return request ? kRequest : kUnknown;
    case kError:
    case kReady:
    case kAuthenticate:
    case kSupported:
    case kResult:
    case kEvent:
    case kAuthChallenge:
    case kAuthSuccess:
      return !request ? kResponse : kUnknown;
    default:
      return kUnknown;
  }
}

// TODO(yzhao): This is for initial development use. Later we need to combine with more inference
// code, as the startup message only appears at the beginning of the exchanges between PostgreSQL
// client and server.
static __inline enum MessageType infer_pgsql_startup_message(const char* buf, size_t count) {
  // Length field: int32, protocol version field: int32, "user" string, 4 bytes.
  const int kMinMsgLen = 4 + 4 + 4;
  if (count < kMinMsgLen) {
    return kUnknown;
  }

  // Assume startup message wont be larger than 10240 (10KiB).
  const int kMaxMsgLen = 10240;
  const int32_t length = read_big_endian_int32(buf);
  if (length < kMinMsgLen) {
    return kUnknown;
  }
  if (length > kMaxMsgLen) {
    return kUnknown;
  }

  const char kPgsqlVer30[] = "\x00\x03\x00\x00";
  if (bpf_strncmp((const char*)buf + 4, kPgsqlVer30, 4) != 0) {
    return kUnknown;
  }

  const char kPgsqlUser[] = "user";
  if (bpf_strncmp((const char*)buf + 8, kPgsqlUser, 4) != 0) {
    return kUnknown;
  }

  return kRequest;
}

// Regular message format: | byte tag | int32_t len | string payload |
static __inline enum MessageType infer_pgsql_query_message(const char* buf, size_t count) {
  const uint8_t kTagQ = 'Q';
  if (*buf != kTagQ) {
    return kUnknown;
  }
  const int32_t len = read_big_endian_int32(buf + 1);
  // The length field include the field itself of 4 bytes. Also the minimal size command is
  // COPY/MOVE. The minimal length is therefore 8.
  const int32_t kMinPayloadLen = 8;
  // Assume typical query message size is below an artificial limit.
  // 30000 is copied from postgres code base:
  // https://github.com/postgres/postgres/tree/master/src/interfaces/libpq/fe-protocol3.c#L94
  const int32_t kMaxPayloadLen = 30000;
  if (len < kMinPayloadLen || len > kMaxPayloadLen) {
    return kUnknown;
  }
  // If the input includes a whole message (1 byte tag + length), check the last character.
  if ((len + 1 <= (int)count) && (buf[len] != '\0')) {
    return kUnknown;
  }
  return kRequest;
}

// TODO(yzhao): ReadyForQuery message could be nice pattern to check, as it has 6 bytes of fixed bit
// pattern, plus one byte of enum with possible values 'I', 'E', 'T'.  But it's usually sent as a
// suffix of a query response, so it's difficult to capture. Research more to see if we can detect
// this message.

static __inline enum MessageType infer_pgsql_regular_message(const char* buf, size_t count) {
  const int kMinMsgLen = 1 + sizeof(int32_t);
  if (count < kMinMsgLen) {
    return kUnknown;
  }
  return infer_pgsql_query_message(buf, count);
}

static __inline enum MessageType infer_pgsql_message(const char* buf, size_t count) {
  enum MessageType type = infer_pgsql_startup_message(buf, count);
  if (type != kUnknown) {
    return type;
  }
  return infer_pgsql_regular_message(buf, count);
}

// MySQL packet:
//      0         8        16        24        32
//      +---------+---------+---------+---------+
//      |        payload_length       | seq_id  |
//      +---------+---------+---------+---------+
//      |                                       |
//      .            ...  body ...              .
//      .                                       .
//      .                                       .
//      +----------------------------------------
// TODO(oazizi/yzhao): This produces too many false positives. Add stronger protocol detection.
static __inline enum MessageType infer_mysql_message(const char* buf, size_t count) {
  static const uint8_t kComQuery = 0x03;
  static const uint8_t kComConnect = 0x0b;
  static const uint8_t kComStmtPrepare = 0x16;
  static const uint8_t kComStmtExecute = 0x17;
  static const uint8_t kComStmtClose = 0x19;

  // MySQL packets start with a 3-byte packet length and a 1-byte packet number.
  // The 5th byte on a request contains a command that tells the type.
  if (count < 5) {
    return kUnknown;
  }

  // Convert 3-byte length to uint32_t.
  // NOLINTNEXTLINE: readability/casting
  uint32_t len = *((uint32_t*)buf);
  len = len & 0x00ffffff;

  uint8_t seq = buf[3];
  uint8_t com = buf[4];

  // The packet number of a request should always be 0.
  if (seq != 0) {
    return kUnknown;
  }

  // No such thing as a zero-length request in MySQL protocol.
  if (len == 0) {
    return kUnknown;
  }

  // Assuming that the length of a request is less than 10k characters to avoid false
  // positive flagging as MySQL, which statistically happens frequently for a single-byte
  // check.
  if (len > 10000) {
    return kUnknown;
  }

  // TODO(oazizi): Consider adding more commands (0x00 to 0x1f).
  // Be careful, though: trade-off is higher rates of false positives.
  if (com == kComConnect || com == kComQuery || com == kComStmtPrepare || com == kComStmtExecute ||
      com == kComStmtClose) {
    return kRequest;
  }
  return kUnknown;
}

static __inline enum MessageType infer_http2_message(const char* buf, size_t count) {
  // Technically, HTTP2 client connection preface is 24 octets [1]. Practically,
  // the first 3 shall be sufficient. Note this is sent from client,
  // so it would be captured on server's read()/recvfrom()/recvmsg() or client's
  // write()/sendto()/sendmsg().
  //
  // [1] https://http2.github.io/http2-spec/#ConnectionHeader
  if (count < 3) {
    return kUnknown;
  }

  if (buf[0] == 'P' && buf[1] == 'R' && buf[2] == 'I') {
    return kRequest;
  }

  if (looks_like_grpc_req_http2_headers_frame(buf, count)) {
    return kRequest;
  }

  return kUnknown;
}

static __inline enum MessageType infer_dns_message(const char* buf, size_t count) {
  const int kDNSHeaderSize = 12;

  // Use the maximum *guaranteed* UDP packet size as the max DNS message size.
  // UDP packets can be larger, but this is the typical maximum size for DNS.
  const int kMaxDNSMessageSize = 512;

  // Maximum number of resource records.
  // https://stackoverflow.com/questions/6794926/how-many-a-records-can-fit-in-a-single-dns-response
  const int kMaxNumRR = 25;

  if (count < kDNSHeaderSize || count > kMaxDNSMessageSize) {
    return kUnknown;
  }

  const uint8_t* ubuf = (const uint8_t*)buf;

  uint16_t flags = (ubuf[2] << 8) + ubuf[3];
  uint16_t num_questions = (ubuf[4] << 8) + ubuf[5];
  uint16_t num_answers = (ubuf[6] << 8) + ubuf[7];
  uint16_t num_auth = (ubuf[8] << 8) + ubuf[9];
  uint16_t num_addl = (ubuf[10] << 8) + ubuf[11];

  bool qr = (flags >> 15) & 0x1;
  uint8_t opcode = (flags >> 11) & 0xf;
  uint8_t zero = (flags >> 6) & 0x1;

  if (zero != 0) {
    return kUnknown;
  }

  if (opcode != 0) {
    return kUnknown;
  }

  if (num_questions == 0 || num_questions > 10) {
    return kUnknown;
  }

  uint32_t num_rr = num_questions + num_answers + num_auth + num_addl;
  if (num_rr > kMaxNumRR) {
    return kUnknown;
  }

  return (qr == 0) ? kRequest : kResponse;
}

// Redis request and response messages share the same format.
// See https://redis.io/topics/protocol for the REDIS protocol spec.
//
// TODO(yzhao): Apply simplified parsing to read the content to distinguished request & response.
static __inline bool is_redis_message(const char* buf, size_t count) {
  // Redis messages start with an one-byte type marker, and end with \r\n terminal sequence.
  if (count < 3) {
    return false;
  }

  const char first_byte = buf[0];

  if (  // Simple strings start with +
      first_byte != '+' &&
      // Errors start with -
      first_byte != '-' &&
      // Integers start with :
      first_byte != ':' &&
      // Bulk strings start with $
      first_byte != '$' &&
      // Arrays start with *
      first_byte != '*') {
    return false;
  }

  // The last two chars are \r\n, the terminal sequence of all Redis messages.
  if (buf[count - 2] != '\r') {
    return false;
  }
  if (buf[count - 1] != '\n') {
    return false;
  }

  return true;
}

static __inline struct protocol_message_t infer_protocol(const char* buf, size_t count) {
  struct protocol_message_t inferred_message;
  inferred_message.protocol = kProtocolUnknown;
  inferred_message.type = kUnknown;

  if ((inferred_message.type = infer_http_message(buf, count)) != kUnknown) {
    inferred_message.protocol = kProtocolHTTP;
  } else if ((inferred_message.type = infer_cql_message(buf, count)) != kUnknown) {
    inferred_message.protocol = kProtocolCQL;
  } else if ((inferred_message.type = infer_pgsql_message(buf, count)) != kUnknown) {
    inferred_message.protocol = kProtocolPGSQL;
  } else if ((inferred_message.type = infer_mysql_message(buf, count)) != kUnknown) {
    inferred_message.protocol = kProtocolMySQL;
  } else if ((inferred_message.type = infer_dns_message(buf, count)) != kUnknown) {
    inferred_message.protocol = kProtocolDNS;
  }

  return inferred_message;
}
