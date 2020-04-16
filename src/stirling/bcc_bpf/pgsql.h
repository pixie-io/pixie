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

  const uint8_t kPgsqlVer30[] = {'\x0', '\x03', '\x00', '\x00'};
  if (bpf_strncmp(buf + 4, kPgsqlVer30, 4) != 0) {
    return kUnknown;
  }

  const char kPgsqlUser[] = "user";
  if (bpf_strncmp(buf + 8, kPgsqlUser, 4) != 0) {
    return kUnknown;
  }

  return kRequest;
}
