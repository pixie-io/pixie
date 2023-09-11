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

#include <string>
#include <utility>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mongodb {

enum class Type : int32_t {
  kOPMsg = 2013,
  kOPReply = 1,
  kOPUpdate = 2001,
  kOPInsert = 2002,
  kReserved = 2003,
  kOPQuery = 2004,
  kOPGetMore = 2005,
  kOPDelete = 2006,
  kOPKillCursors = 2007,
  kOPCompressed = 2012,
};

constexpr uint8_t kHeaderLength = 16;
constexpr uint8_t kMessageLengthSize = 4;
constexpr uint8_t kSectionLengthSize = 4;

struct Section {
  uint8_t kind = 0;
  int32_t length = 0;
  std::vector<std::string> documents;
};

// Types of OP_MSG requests/responses
constexpr std::string_view insert = "insert";
constexpr std::string_view delete_ = "delete";
constexpr std::string_view update = "update";
constexpr std::string_view find = "find";
constexpr std::string_view cursor = "cursor";
constexpr std::string_view ok = "ok";

constexpr int32_t kMaxBSONOBjSize = 16000000;

/**
 * MongoDB's Wire Protocol documentation can be found here:
 * https://www.mongodb.com/docs/manual/reference/mongodb-wire-protocol/#std-label-wire-msg-sections
 *
 * The header for a standard message looks like:
 * -----------------------------------------------------------------------
 * |  int32 length  | int32 requestID | int32 responseTo|  int32 opCode  |
 * -----------------------------------------------------------------------
 *
 * Documentation regarding OP_MSG frames can be found here:
 * https://github.com/mongodb/specifications/blob/master/source/message/OP_MSG.rst#sections
 *
 * A frame of type OP_MSG looks like:
 * -----------------------------------------------------------------------
 * |  int32 length  | int32 requestID | int32 responseTo|  int32 opCode  |
 * -----------------------------------------------------------------------
 * | uint32 flagBits|  Sections[] sections  |  optional uint32 checksum  |
 * -----------------------------------------------------------------------
 *
 * There must be only one payload section of kind 0 and any number of payload sections of kind 1.
 *
 * A payload section of kind 0 looks like:
 * -----------------------------------------------------------------------
 * |  uint8 kind | int32 document payload length |        document       |
 * -----------------------------------------------------------------------
 *
 * A payload section of kind 1 looks like:
 * -----------------------------------------------------------------------
 * |  uint8 kind  |               int32 section length                   |
 * -----------------------------------------------------------------------
 * |       identifier (command argument that ends with \x00)             |
 * -----------------------------------------------------------------------
 * | int32 document payload length |              document               |
 * -----------------------------------------------------------------------
 *
 * There can be 0 or more documents in a section of kind 1 without a separator between them.
 */

struct Frame : public FrameBase {
  // Message Header Fields
  // Length of the mongodb header and the wire protocol data.
  int32_t length = 0;
  int32_t request_id = 0;
  int32_t response_to = 0;
  int32_t op_code = 0;

  // OP_MSG Fields
  // Relevant flag bits
  bool checksum_present = false;
  bool more_to_come = false;
  bool exhaust_allowed = false;
  std::vector<Section> sections;
  std::string op_msg_type;
  uint32_t checksum = 0;

  bool consumed = false;
  size_t ByteSize() const override { return sizeof(Frame); }

  std::string ToString() const override {
    return absl::Substitute(
        "MongoDB message [length=$0 requestID=$1 responseTo=$2 opCode=$3 checksum=$4]", length,
        request_id, response_to, op_code, checksum);
  }
};

struct Record {
  Frame req;
  Frame resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = NoState;
};

}  // namespace mongodb
}  // namespace protocols
}  // namespace stirling
}  // namespace px
