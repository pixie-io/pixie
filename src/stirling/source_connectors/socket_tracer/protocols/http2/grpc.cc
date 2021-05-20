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

#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"

#include <utility>

#include <google/protobuf/empty.pb.h>
#include <google/protobuf/text_format.h>

#include "src/common/base/base.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace grpc {

using ::google::protobuf::Empty;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;

namespace {

// Text format protobuf are not valid JSON. One obvious issue is that TextFormat uses unquoted
// field numbers as key: {1: "some string"}, which is not allowed in JSON.
Status PBWireToText(std::string_view message, TextFormat::Printer* pb_printer, std::string* text) {
  Empty empty_pb;
  const bool parse_succeeded = empty_pb.ParsePartialFromArray(message.data(), message.size());
  // Proceed to print text format protobuf even if the parse failed. This allows producing partial
  // message.
  const bool print_succeeded = pb_printer->PrintToString(empty_pb, text);
  if (!parse_succeeded) {
    return error::InvalidArgument("Failed to parse the serialized protobuf message");
  }
  if (!print_succeeded) {
    return error::InvalidArgument("Failed to print protobuf message to text format");
  }
  return Status::OK();
}

// Parses the gRPC payload into text format protobuf. In addition to parsing protobuf messages, this
// function extracts compression and length field, and also handles multiple concatenated payloads
// as well.
Status GRPCPBWireToText(std::string_view message, std::string* text,
                        std::optional<int> str_truncation_len) {
  if (message.size() < kGRPCMessageHeaderSizeInBytes) {
    return error::InvalidArgument(
        "The gRPC message does not have enough data. "
        "Might be resulted from early termination of invalid RPC calls. "
        "E.g.: calling unimplemented method");
  }

  BinaryDecoder decoder(message);

  TextFormat::Printer pb_printer;
  pb_printer.SetTruncateStringFieldLongerThan(str_truncation_len.value_or(0));

  Status status;

  while (!decoder.eof()) {
    PL_ASSIGN_OR_RETURN(uint8_t compressed_flag, decoder.ExtractInt<uint8_t>());
    if (compressed_flag == 1) {
      return error::Unimplemented("Compressed data is not implemented");
    }
    PL_ASSIGN_OR_RETURN(uint32_t len, decoder.ExtractInt<uint32_t>());
    // Only extract remaining data if the data is truncated.
    PL_ASSIGN_OR_RETURN(std::string_view data, decoder.ExtractString<char>(std::min(
                                                   static_cast<size_t>(len), decoder.BufSize())));

    std::string pb_str;
    // Include the most recent status.
    status = PBWireToText(data, &pb_printer, &pb_str);

    text->append(pb_str);
  }
  return status;
}

}  // namespace

// TODO(yzhao): Support reflection to get message types instead of empty message.
std::string ParsePB(std::string_view str, std::optional<int> str_truncation_len) {
  std::string text;
  Status s = GRPCPBWireToText(str, &text, str_truncation_len);
  absl::StripTrailingAsciiWhitespace(&text);
  if (!s.ok() && text.empty()) {
    return "<Failed to parse protobuf>";
  }
  return text;
}

}  // namespace grpc
}  // namespace stirling
}  // namespace px
