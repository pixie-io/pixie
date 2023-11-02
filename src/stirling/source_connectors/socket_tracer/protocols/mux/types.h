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

#include <cstdint>
#include <string>
#include <utility>

#include <absl/container/flat_hash_map.h>
#include <magic_enum.hpp>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

enum class Type : int8_t {
  kTreq = 1,
  kRreq = -1,
  kTdispatch = 2,
  kRdispatch = -2,

  // control messages
  kTdrain = 64,
  kRdrain = -64,
  kTping = 65,
  kRping = -65,

  kTdiscarded = 66,
  kRdiscarded = -66,

  kTlease = 67,

  kTinit = 68,
  kRinit = -68,

  kRerr = -128,

  // only used to preserve backwards compatibility
  kTdiscardedOld = -62,
  kRerrOld = 127,
};

inline bool IsMuxType(int8_t t) {
  std::optional<Type> mux_type = magic_enum::enum_cast<Type>(t);
  return mux_type.has_value();
}

inline StatusOr<Type> GetMatchingRespType(Type req_type) {
  switch (req_type) {
    case Type::kRerrOld:
      return Type::kRerrOld;
    case Type::kRerr:
      return Type::kRerr;
    case Type::kTinit:
      return Type::kRinit;
    case Type::kTping:
      return Type::kRping;
    case Type::kTreq:
      return Type::kRreq;
    case Type::kTdrain:
      return Type::kRdrain;
    case Type::kTdispatch:
      return Type::kRdispatch;
    case Type::kTdiscardedOld:
    case Type::kTdiscarded:
      return Type::kRdiscarded;
    default:
      return error::Internal("Unexpected request type $0", magic_enum::enum_name(req_type));
  }
}

/**
 * The mux protocol is explained in more detail in the finagle source code
 * here
 * (https://github.com/twitter/finagle/blob/release/finagle-mux/src/main/scala/com/twitter/finagle/mux/package.scala)
 *
 * Mux messages can take on a few different wire formats. Each type
 * is described below. All fields are big endian.
 *
 * Regular message
 * ----------------------------------------------
 * | uint32 header size | int8 type | int24 tag |
 * ----------------------------------------------
 * |                 Payload                    |
 * ----------------------------------------------
 *
 * Rinit message
 * ----------------------------------------------
 * | uint32 header size | int8 type | int24 tag |
 * ----------------------------------------------
 * |                   Why                      |
 * ----------------------------------------------
 *
 * Rdispatch / Tdispatch (Tdispatch does not have reply status)
 *
 * reply status is one of ok (0), error (1) or nack (2)
 * https://github.com/twitter/finagle/blob/2e4d56d93a2bdfb63e293d84727d5266d2969f01/finagle-mux/src/main/scala/com/twitter/finagle/mux/transport/Message.scala#L549-L552
 * ----------------------------------------------
 * | uint32 header size | int8 type | int24 tag |
 * ----------------------------------------------
 * |            uint8 reply status              |
 * ----------------------------------------------
 * | uint16 # context | uint16 ctx key length   |
 * ----------------------------------------------
 * | ctx key          | uint16 ctx value length |
 * ----------------------------------------------
 * | ctx value        | uint16 ctx value length |
 * ----------------------------------------------
 * | uint16 destination length | uint16 # dtabs |
 * ----------------------------------------------
 * | uint16 source len |         source         |
 * ----------------------------------------------
 * | uint16 dest len   |       destination      |
 * ----------------------------------------------
 */
struct Frame : public FrameBase {
  // The length of the mux header and the application protocol data excluding
  // the 4 byte length field. For Tdispatch / Rdispatch messages when using a
  // protocol like thriftmux, this would include the length of the mux and thrift
  // data.
  uint32_t length = 0;
  int8_t type = 0;
  uint24_t tag = 0;
  std::string why;
  bool consumed = false;
  // Reply status codes. Only present in Rdispatch messages types
  int8_t reply_status = 0;

  const auto& context() const { return context_; }

  void InsertContext(std::string_view ctx_key,
                     absl::flat_hash_map<std::string, std::string> value) {
    CTX_DCHECK(context_.find(ctx_key) == context_.end());

    context_size_ += ctx_key.size();
    for (const auto& [k, v] : value) {
      context_size_ += k.size() + v.size();
    }

    context_[ctx_key] = std::move(value);
  }

  size_t ByteSize() const override { return sizeof(Frame) + why.size() + context_size_; }

  /*
   * Returns the number of bytes remaining in the mux body / payload
   * after parsing the required fields for all packets: header size,
   * type and tag.
   *
   * Since mux's header size field is not included in the size field
   * this will be 4 bytes less the length member (to account for type
   * and tag fields).
   *
   * This is typically used when reading the rest of the payload for
   * the RerrOld, Rerr, Rinit and Tinit messages that contain a why
   * message, tls or compression parameters, etc.
   */
  size_t MuxBodyLength() const { return length - 4; }

  // TODO(ddelnano): Include printing the context, dtabs and other fields
  std::string ToString() const override {
    return absl::Substitute("Mux message [len=$0 type=$1 tag=$2 # context: TBD dtabs: TBD]", length,
                            type, uint32_t(tag));
  }

 private:
  size_t context_size_ = 0;
  absl::flat_hash_map<std::string, absl::flat_hash_map<std::string, std::string>> context_;
};

struct Record {
  Frame req;
  Frame resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

using stream_id_t = uint16_t;
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  // TODO(ddelnano): mux does have state but assume no state for now
  using state_type = NoState;
  using key_type = stream_id_t;
};

}  // namespace mux
}  // namespace protocols
}  // namespace stirling
}  // namespace px
