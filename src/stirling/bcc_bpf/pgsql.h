// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.
#pragma once

#include "src/stirling/bcc_bpf/utils.h"
#include "src/stirling/bcc_bpf_interface/common.h"

// TODO(yzhao): This is for initial development use. Later we need to combine with more inference
// code, as the startup message only appears at the beginning of the exchanges between PostgreSQL
// client and server.
static __inline enum MessageType infer_pgsql_startup_message(const uint8_t* buf, size_t count) {
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
static __inline enum MessageType infer_pgsql_query_message(const uint8_t* buf, size_t count) {
  const uint8_t kTagQ = 'Q';
  if (*buf != kTagQ) {
    return kUnknown;
  }
  const int32_t len = read_big_endian_int32(buf + 1);
  // The length field include the field itself of 4 bytes. Also the minimal size command is
  // COPY/MOVE. The minimal length is therefore 8.
  const int32_t kMinPayloadLen = 8;
  // Assume typical query message size is below an artificial limit.
  const int32_t kMaxPayloadLen = 1024;
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

static __inline enum MessageType infer_pgsql_regular_message(const uint8_t* buf, size_t count) {
  const int kMinMsgLen = 1 + sizeof(int32_t);
  if (count < kMinMsgLen) {
    return kUnknown;
  }
  return infer_pgsql_query_message(buf, count);
}

static __inline enum MessageType infer_pgsql_message(const uint8_t* buf, size_t count) {
  enum MessageType type = infer_pgsql_startup_message(buf, count);
  if (type != kUnknown) {
    return type;
  }
  return infer_pgsql_regular_message(buf, count);
}
