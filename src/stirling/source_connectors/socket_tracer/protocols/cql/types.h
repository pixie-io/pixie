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

#include <chrono>
#include <optional>
#include <string>

#include <magic_enum.hpp>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase.

namespace px {
namespace stirling {
namespace protocols {
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

inline bool IsReqOpcode(Opcode opcode) {
  std::optional<ReqOp> req_opcode = magic_enum::enum_cast<ReqOp>(static_cast<int>(opcode));
  return req_opcode.has_value();
}

inline bool IsRespOpcode(Opcode opcode) {
  std::optional<RespOp> resp_opcode = magic_enum::enum_cast<RespOp>(static_cast<int>(opcode));
  return resp_opcode.has_value();
}

using stream_id_t = uint16_t;
struct FrameHeader {
  // Top bit is direction.
  uint8_t version;
  uint8_t flags;
  stream_id_t stream;
  Opcode opcode;
  int32_t length;
};

struct Frame : public FrameBase {
  FrameHeader hdr;
  std::string msg;
  bool consumed = false;

  size_t ByteSize() const override { return sizeof(Frame) + msg.size(); }
};

constexpr int kFrameHeaderLength = 9;

// Max length is 256MB. See section 2.5 of the spec.
constexpr int kMaxFrameLength = 256 * 1024 * 1024;

// Mask to apply to FrameHeader::version to get the version.
// The top bit is the req/response direction, and should not be used.
constexpr uint8_t kVersionMask = 0x7f;
constexpr uint8_t kDirectionMask = 0x80;

// Currently only support version 3 and 4 of the protocol,
// which appear to be the most popular versions.
constexpr uint8_t kMinSupportedProtocolVersion = 3;
constexpr uint8_t kMaxSupportedProtocolVersion = 4;

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

struct Request {
  ReqOp op;

  // The body of the request, if request has a single string parameter. Otherwise empty for now.
  std::string msg;

  // Timestamp of the request packet.
  uint64_t timestamp_ns;

  std::string ToString() const {
    return absl::Substitute("op=$0 msg=$1 timestamp_ns=$2", magic_enum::enum_name(op), msg,
                            timestamp_ns);
  }
};

struct Response {
  RespOp op;

  // Any relevant response message.
  std::string msg;

  // Timestamp of the response packet.
  uint64_t timestamp_ns;

  std::string ToString() const {
    return absl::Substitute("op=$0 msg=$1 timestamp_ns=$2", magic_enum::enum_name(op), msg,
                            timestamp_ns);
  }
};

/**
 *  Record is the primary output of the cql parser.
 */
struct Record {
  Request req;
  Response resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = NoState;
  using key_type = stream_id_t;
  static constexpr StreamSupport stream_support = BaseProtocolTraits<Record>::UseStream;
};

}  // namespace cass

template <>
cass::stream_id_t GetStreamID(cass::Frame* frame);

}  // namespace protocols
}  // namespace stirling
}  // namespace px
