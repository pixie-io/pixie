#pragma once

#include <string>

#include "src/stirling/utils/req_resp_pair.h"

namespace pl {
namespace stirling {
namespace cass {

// Cassandra spec is used to define many of the types in this header.
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec

//-----------------------------------------------------------------------------
// Cassandra Frame
//-----------------------------------------------------------------------------

enum class Opcode : uint8_t {
  kError = 0x00,
  kStartup = 0x01,
  kReady = 0x02,
  kAuthenticate = 0x03,
  kOptions = 0x05,
  kSupported = 0x06,
  kQuery = 0x07,
  kResult = 0x08,
  kPrepare = 0x09,
  kExecute = 0x0a,
  kRegister = 0x0b,
  kEvent = 0x0c,
  kBatch = 0x0d,
  kAuthChallenge = 0x0e,
  kAuthResponse = 0x0f,
  kAuthSuccess = 0x10,
};

enum class ReqOp : uint8_t {
  kStartup = static_cast<uint8_t>(Opcode::kStartup),
  kAuthResponse = static_cast<uint8_t>(Opcode::kAuthResponse),
  kOptions = static_cast<uint8_t>(Opcode::kOptions),
  kQuery = static_cast<uint8_t>(Opcode::kQuery),
  kPrepare = static_cast<uint8_t>(Opcode::kPrepare),
  kExecute = static_cast<uint8_t>(Opcode::kExecute),
  kBatch = static_cast<uint8_t>(Opcode::kBatch),
  kRegister = static_cast<uint8_t>(Opcode::kRegister),
};

enum class RespOp : uint8_t {
  kError = static_cast<uint8_t>(Opcode::kError),
  kReady = static_cast<uint8_t>(Opcode::kReady),
  kAuthenticate = static_cast<uint8_t>(Opcode::kAuthenticate),
  kSupported = static_cast<uint8_t>(Opcode::kSupported),
  kResult = static_cast<uint8_t>(Opcode::kResult),
  kEvent = static_cast<uint8_t>(Opcode::kEvent),
  kAuthChallenge = static_cast<uint8_t>(Opcode::kAuthChallenge),
  kAuthSuccess = static_cast<uint8_t>(Opcode::kAuthSuccess),
};

struct FrameHeader {
  // Top bit is direction.
  uint8_t version;
  uint8_t flags;
  uint16_t stream;
  Opcode opcode;
  int32_t length;
};

struct Frame {
  FrameHeader hdr;
  std::string msg;
  uint64_t timestamp_ns = 0;
  bool consumed = false;
};

constexpr int kFrameHeaderLength = 9;

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

struct Request {
  ReqOp op;

  // The body of the request, if request has a single string parameter. Otherwise empty for now.
  std::string msg;

  // Timestamp of the request packet.
  uint64_t timestamp_ns;
};

struct Response {
  RespOp op;

  // Any relevant response message.
  std::string msg;

  // Timestamp of the response packet.
  uint64_t timestamp_ns;
};

/**
 *  Record is the primary output of the mysql parser.
 */
using Record = ReqRespPair<Request, Response>;

}  // namespace cass
}  // namespace stirling
}  // namespace pl
