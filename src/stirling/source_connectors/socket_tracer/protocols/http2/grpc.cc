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
#include "src/common/zlib/zlib_wrapper.h"
#include "src/stirling/utils/binary_decoder.h"

DEFINE_bool(socket_tracer_enable_http2_gzip, false,
            "If true, decompress gzipped request and response bodies of HTTP2 messages.");

namespace px {
namespace stirling {
namespace grpc {

using ::google::protobuf::Empty;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::px::stirling::protocols::http2::HalfStream;
using ::px::stirling::protocols::http2::Stream;

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
Status GRPCPBWireToText(std::string_view message, bool is_gzipped, std::string* text,
                        std::optional<int> str_field_truncation_len) {
  // 1 byte compression flag, and 4 bytes length field.
  constexpr size_t kGRPCMessageHeaderSizeBytes = 1 + sizeof(int32_t);
  if (message.size() < kGRPCMessageHeaderSizeBytes) {
    return error::InvalidArgument(
        "The gRPC message does not have enough data. "
        "Might be resulted from early termination of invalid RPC calls. "
        "E.g.: calling unimplemented method");
  }

  BinaryDecoder decoder(message);

  TextFormat::Printer pb_printer;
  pb_printer.SetTruncateStringFieldLongerThan(str_field_truncation_len.value_or(0));

  Status status;

  while (!decoder.eof()) {
    PX_ASSIGN_OR_RETURN(uint8_t compressed_flag, decoder.ExtractBEInt<uint8_t>());
    // gRPC spec states that it's OK to *not* compress even if grpc-encoding header has specified
    // compression algorithm, which is indicated by is_gzipped.
    bool is_compressed = compressed_flag == 1;
    if (is_compressed && !is_gzipped) {
      text->append("<Non-gzip decompression not supported>");
      continue;
    }

    PX_ASSIGN_OR_RETURN(uint32_t len, decoder.ExtractBEInt<uint32_t>());
    // Only extract remaining data if the data is truncated. Protobuf parsing produces a partial
    // result if the data is incomplete, so potential truncation could be tolerated.
    PX_ASSIGN_OR_RETURN(std::string_view data, decoder.ExtractString<char>(std::min(
                                                   static_cast<size_t>(len), decoder.BufSize())));

    if (is_compressed && is_gzipped && !FLAGS_socket_tracer_enable_http2_gzip) {
      text->append("<GZip decompression is disabled>");
      continue;
    }

    std::string gunzipped_data;
    if (is_compressed && is_gzipped) {
      auto data_or = px::zlib::Inflate(data);
      if (data_or.ok()) {
        gunzipped_data = data_or.ConsumeValueOrDie();
      } else {
        text->append("<Failed to gunzip data>");
        continue;
      }
    }
    std::string pb_str;
    // Include the most recent status.
    status = PBWireToText(is_compressed ? gunzipped_data : data, &pb_printer, &pb_str);

    text->append(pb_str);
  }
  return status;
}

}  // namespace

// TODO(yzhao): Support reflection to get message types instead of empty message.
// TODO(yzhao): This wrapper is too thin, remove.
std::string ParsePB(std::string_view str, bool is_gzipped,
                    std::optional<int> str_field_truncation_len) {
  std::string text;
  Status s = GRPCPBWireToText(str, is_gzipped, &text, str_field_truncation_len);
  absl::StripTrailingAsciiWhitespace(&text);
  if (!s.ok() && text.empty()) {
    return "<Failed to parse protobuf>";
  }
  return text;
}

void ParseReqRespBody(px::stirling::protocols::http2::Stream* http2_stream,
                      std::string_view truncation_suffix,
                      std::optional<int> str_field_truncation_len) {
  bool has_grpc_encoding = http2_stream->HasGRPCEncodingHeader();
  bool is_gzipped = http2_stream->HasGZipGRPCEncoding();
  if (has_grpc_encoding && !is_gzipped) {
    // Don't do anything if the compression is done with an unsupported algorithm.
    return;
  }
  if (http2_stream->HasGRPCContentType()) {
    *http2_stream->send.mutable_data() =
        ParsePB(http2_stream->send.data(), is_gzipped, str_field_truncation_len);
    *http2_stream->recv.mutable_data() =
        ParsePB(http2_stream->recv.data(), is_gzipped, str_field_truncation_len);
  }
  if (http2_stream->send.data_truncated()) {
    http2_stream->send.mutable_data()->append(truncation_suffix);
  }
  if (http2_stream->recv.data_truncated()) {
    http2_stream->recv.mutable_data()->append(truncation_suffix);
  }
}

}  // namespace grpc
}  // namespace stirling
}  // namespace px
