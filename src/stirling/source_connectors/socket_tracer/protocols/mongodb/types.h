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
  kOPReply = 1,
  kOPUpdate = 2001,
  kOPInsert = 2002,
  kReserved = 2003,
  kOPQuery = 2004,
  kOPGetMore = 2005,
  kOPDelete = 2006,
  kOPKillCursors = 2007,
  kOPCompressed = 2012,
  kOPMsg = 2013,
};

constexpr uint8_t kHeaderLength = 16;
constexpr uint8_t kMessageLengthSize = 4;
constexpr uint8_t kSectionLengthSize = 4;
constexpr uint8_t kHeaderAndFlagSize = 20;

constexpr uint32_t kChecksumBitmask = 1;
constexpr uint32_t kMoreToComeBitmask = 1 << 1;
constexpr uint32_t kExhaustAllowedBitmask = 1 << 16;
// Bits 2-15 must not be set, this bitmask sets the least significant 16 bits except bits 0, 1.
constexpr uint32_t kRequiredUnsetBitmask = 0xFFFC;

struct Section {
  uint8_t kind = 0;
  int32_t length = 0;
  std::vector<std::string> documents;
};

constexpr uint8_t kSectionKindSize = 1;

enum class SectionKind : uint8_t {
  kSectionKindZero = 0,
  kSectionKindOne = 1,
};

// Types of OP_MSG requests/responses
constexpr std::string_view kInsert = "insert";
constexpr std::string_view kDelete = "delete";
constexpr std::string_view kUpdate = "update";
constexpr std::string_view kFind = "find";
constexpr std::string_view kCursor = "cursor";
constexpr std::string_view kOk = "ok";

// Types of top level keys for handshaking messages
constexpr std::string_view kHello = "hello";
constexpr std::string_view kIsMaster = "isMaster";
constexpr std::string_view kIsMasterAlternate = "ismaster";

constexpr int32_t kMaxBSONObjSize = 16000000;

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
 * Information about the identifiers can be found here:
 * https://github.com/mongodb/specifications/blob/e09b41df206f9efaa36ba4c332c47d04ddb7d6d1/source/message/OP_MSG.rst#command-arguments-as-payload
 *
 * There can be 0 or more documents in a section of kind 1 without a separator between them.
 *
 * Information about MongoDB handshaking messages can be found here:
 * https://github.com/mongodb/specifications/blob/022fbf64fb36c80b9295ba93acec150c94362767/source/mongodb-handshake/handshake.rst
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
  std::string frame_body;
  uint32_t checksum = 0;
  bool is_handshake = false;

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

// The stream_order state tracks which stream_id to stitch first.
// In more detail, the MongoDB wire protocol can link responses together in a more_to_come scenario
// where each response points to a new response via the streamID. To stitch correctly, we record the
// first streamID on the request side in each such sequence during frame parsing and store that in
// the stream_order vector. The second item in the pair is a boolean flag that is initially false,
// but set to true during the stitching process to indicate that the transaction has been processsed
// and can be removed from the vector.
using stream_id_t = int32_t;
struct State {
  std::vector<std::pair<mongodb::stream_id_t, bool>> stream_order;
};

struct StateWrapper {
  State global;
  std::monostate send;
  std::monostate recv;
};

struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = StateWrapper;
  using key_type = stream_id_t;
  static constexpr StreamSupport stream_support = BaseProtocolTraits<Record>::UseStream;
};

}  // namespace mongodb

template <>
mongodb::stream_id_t GetStreamID(mongodb::Frame* frame);
}  // namespace protocols
}  // namespace stirling
}  // namespace px
