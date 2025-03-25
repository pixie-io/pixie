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

#include "src/stirling/source_connectors/socket_tracer/protocols/nats/parse.h"

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/nats/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace nats {

using ::px::utils::ToJSONString;

// Based on:
// https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#protocol-conventions
constexpr std::string_view kMessageTerminateMarker = "\r\n";
constexpr std::string_view kMessageFieldDelimiters = " \t";

size_t FindMessageBoundary(std::string_view buf, size_t start_pos) {
  // Based on https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md.
  const std::vector<std::string_view> kmessage_type_ts = {kInfo, kConnect, kPub,  kSub, kUnsub,
                                                          kMsg,  kPing,    kPong, kOK,  kERR};
  constexpr ssize_t kMinMsgSize = 3;
  for (ssize_t i = start_pos; i < static_cast<ssize_t>(buf.size()) - kMinMsgSize; ++i) {
    auto buf_substr = buf.substr(i);
    for (auto msg_type : kmessage_type_ts) {
      if (absl::StartsWith(buf_substr, msg_type)) {
        return i;
      }
    }
  }
  return std::string_view::npos;
}

namespace {

class NATSDecoder : public BinaryDecoder {
 public:
  explicit NATSDecoder(std::string_view buf) : BinaryDecoder(buf) {}

  // Returns the command of a NATS message.
  StatusOr<std::string_view> ExtractCommand();
};

StatusOr<std::string_view> NATSDecoder::ExtractCommand() {
  constexpr std::string_view kPingMsg = "PING\r\n";
  constexpr std::string_view kPongMsg = "PONG\r\n";
  constexpr std::string_view kOKMsg = "+OK\r\n";
  if (absl::StartsWith(buf_, kPingMsg)) {
    return kPing;
  }
  if (absl::StartsWith(buf_, kPongMsg)) {
    return kPong;
  }
  if (absl::StartsWith(buf_, kOKMsg)) {
    return kOK;
  }
  size_t pos = buf_.find_first_of(kMessageFieldDelimiters);
  if (pos == std::string_view::npos) {
    return error::NotFound("Could not find space or tab in the buffer.");
  }
  auto cmd = buf_.substr(0, pos);
  buf_.remove_prefix(pos + 1);
  return cmd;
}

// For messages formatted as: <command> <options>\r\n
Status ParseMessageOptions(NATSDecoder* decoder, Message* msg) {
  PX_ASSIGN_OR_RETURN(msg->options, decoder->ExtractStringUntil(kMessageTerminateMarker));
  absl::StripTrailingAsciiWhitespace(&msg->options);
  return Status::OK();
}

StatusOr<std::vector<std::string_view>> GetMessageFields(NATSDecoder* decoder) {
  PX_ASSIGN_OR_RETURN(std::string_view msg_str,
                      decoder->ExtractStringUntil(kMessageTerminateMarker));
  std::vector<std::string_view> msg_fields =
      absl::StrSplit(msg_str, absl::ByAnyChar(kMessageFieldDelimiters), absl::SkipEmpty());
  return msg_fields;
}

// The name zip is borrowed from Python. Returns a map with the keys and values provided in the
// vectors.
std::map<std::string_view, std::string_view> Zip(const std::vector<std::string_view>& keys,
                                                 const std::vector<std::string_view>& values) {
  std::map<std::string_view, std::string_view> res;
  for (size_t i = 0; i < keys.size() && i < values.size(); ++i) {
    res[keys[i]] = values[i];
  }
  return res;
}

Status ParseMessageWithFields(NATSDecoder* decoder,
                              const std::vector<std::string_view>& msg_field_names, Message* msg) {
  PX_ASSIGN_OR_RETURN(std::vector<std::string_view> msg_fields, GetMessageFields(decoder));
  msg->options = ToJSONString(Zip(msg_field_names, msg_fields));
  return Status::OK();
}

Status ParseSubOptions(NATSDecoder* decoder, Message* msg) {
  PX_ASSIGN_OR_RETURN(std::vector<std::string_view> msg_fields, GetMessageFields(decoder));

  constexpr size_t kFieldNumWithQueueGroup = 3;
  constexpr std::string_view kSubjectKey = "subject";
  constexpr std::string_view kQueueGroupKey = "queue group";
  constexpr std::string_view kSIDKey = "sid";

  if (msg_fields.size() == kFieldNumWithQueueGroup) {
    msg->options = ToJSONString(Zip({kSubjectKey, kQueueGroupKey, kSIDKey}, msg_fields));
  } else if (msg_fields.size() == kFieldNumWithQueueGroup - 1) {
    msg->options = ToJSONString(Zip({kSubjectKey, kSIDKey}, msg_fields));
  } else {
    return error::InvalidArgument("Incorrect number of fields: $0", msg_fields.size());
  }

  return Status::OK();
}

Status ParseMessageWithPayload(NATSDecoder* decoder,
                               const std::vector<std::string_view>& msg_field_names, Message* msg) {
  PX_ASSIGN_OR_RETURN(std::string_view msg_str,
                      decoder->ExtractStringUntil(kMessageTerminateMarker));

  std::vector<std::string_view> msg_fields =
      absl::StrSplit(msg_str, absl::ByAnyChar(kMessageFieldDelimiters));

  // Remove the last field, which is the payload size, which is not needed for parsing.
  msg_fields.pop_back();

  std::map<std::string_view, std::string_view> options = Zip(msg_field_names, msg_fields);

  constexpr std::string_view kPayloadKey = "payload";
  PX_ASSIGN_OR_RETURN(std::string_view payload,
                      decoder->ExtractStringUntil(kMessageTerminateMarker));
  options[kPayloadKey] = payload;

  msg->options = ToJSONString(options);

  return Status::OK();
}

}  // namespace

Status ParseMessage(std::string_view* buf, Message* msg) {
  *buf = absl::StripLeadingAsciiWhitespace(*buf);

  NATSDecoder decoder(*buf);

  PX_ASSIGN_OR_RETURN(msg->command, decoder.ExtractCommand());

  if (msg->command == kInfo) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#info
    PX_RETURN_IF_ERROR(ParseMessageOptions(&decoder, msg));
  } else if (msg->command == kConnect) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#connect
    PX_RETURN_IF_ERROR(ParseMessageOptions(&decoder, msg));
  } else if (msg->command == kSub) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#sub
    PX_RETURN_IF_ERROR(ParseSubOptions(&decoder, msg));
  } else if (msg->command == kUnsub) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#unsub
    PX_RETURN_IF_ERROR(ParseMessageWithFields(&decoder, {"sid", "max_msgs"}, msg));
  } else if (msg->command == kPub) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#pub
    PX_RETURN_IF_ERROR(ParseMessageWithPayload(&decoder, {"subject", "reply-to"}, msg));
  } else if (msg->command == kMsg) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#msg
    PX_RETURN_IF_ERROR(ParseMessageWithPayload(&decoder, {"subject", "sid", "reply-to"}, msg));
  } else if (msg->command == kPing) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#ping
    decoder.ExtractStringUntil(kMessageTerminateMarker);
  } else if (msg->command == kPong) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#pong
    decoder.ExtractStringUntil(kMessageTerminateMarker);
  } else if (msg->command == kOK) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#okerr
    decoder.ExtractStringUntil(kMessageTerminateMarker);
  } else if (msg->command == kERR) {
    // https://github.com/nats-io/docs/blob/master/nats_protocol/nats-protocol.md#okerr
    PX_RETURN_IF_ERROR(ParseMessageOptions(&decoder, msg));
  } else {
    return error::InvalidArgument("Invalid command: $0", *buf);
  }

  *buf = decoder.Buf();
  return Status::OK();
}

}  // namespace nats

template <>
size_t FindFrameBoundary<nats::Message>(message_type_t /*type*/, std::string_view buf,
                                        size_t start_pos, NoState* /*state*/) {
  return nats::FindMessageBoundary(buf, start_pos);
}

template <>
ParseState ParseFrame(message_type_t /*type*/, std::string_view* buf, nats::Message* msg,
                      NoState* /*state*/) {
  return TranslateStatus(nats::ParseMessage(buf, msg));
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
