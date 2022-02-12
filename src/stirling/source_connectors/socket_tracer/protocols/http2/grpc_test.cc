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

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/greet.pb.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/multi_fields.pb.h"

namespace px {
namespace stirling {
namespace grpc {

using ::google::protobuf::TextFormat;
using ::px::stirling::protocols::http2::testing::HelloReply;
using ::px::stirling::protocols::http2::testing::HelloRequest;
using ::px::stirling::protocols::http2::testing::MultiFieldsMessage;
using ::testing::HasSubstr;
using ::testing::StrEq;

std::string PackGRPCMsg(std::string_view serialized_pb) {
  std::string s;
  // Uncompressed.
  s.push_back('\x00');

  uint32_t size_be = htonl(serialized_pb.size());
  std::string_view size_bytes(reinterpret_cast<char*>(&size_be), sizeof(uint32_t));
  s.append(size_bytes);
  s.append(serialized_pb);
  return s;
}

TEST(ParseProtobufTest, VariousCornerCasesInInput) {
  EXPECT_THAT(ParsePB(""), StrEq("<Failed to parse protobuf>"));
  EXPECT_THAT(ParsePB("a"), StrEq("<Failed to parse protobuf>"));
  EXPECT_THAT(ParsePB("aa"), StrEq("<Failed to parse protobuf>"));
  EXPECT_THAT(ParsePB("aaa"), StrEq("<Failed to parse protobuf>"));
  EXPECT_THAT(ParsePB("aaaa"), StrEq("<Failed to parse protobuf>"));
  EXPECT_THAT(ParsePB("aaaaa"), StrEq(""));
  {
    std::string text;
    std::string_view s = CreateStringView<char>("\x00\x00\x00\x00\x00");
    EXPECT_THAT(ParsePB(s), StrEq(""));
    EXPECT_THAT(ParsePB(s), StrEq(""));
  }
  {
    std::string text;
    std::string_view data = "test";
    std::string_view header = CreateStringView<char>("\x00\x00\x00\x00\x04");
    std::string invalid_serialized_pb = absl::StrCat(header, data);
    EXPECT_THAT(ParsePB(invalid_serialized_pb), StrEq("<Failed to parse protobuf>"));
    EXPECT_THAT(ParsePB(invalid_serialized_pb), StrEq("<Failed to parse protobuf>"));
  }
}

TEST(ParsePB, Basic) {
  HelloRequest req;
  req.set_name("pixielabs");
  std::string s = PackGRPCMsg(req.SerializeAsString());
  EXPECT_EQ(ParsePB(s), "1 {\n  14: 105\n  15: 105\n  12: 0x7362616c\n}");
}

TEST(ParsePB, MultipleGRPCLengthPrefixedMessages) {
  std::string_view data = CreateStringView<char>(
      "\x00\x00\x00\x00\x0D\x0A\x0B\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64"
      "\x00\x00\x00\x00\x0D\x0A\x0B\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64"
      "\x00\x00\x00\x00\x0D\x0A\x0B\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64");
  EXPECT_THAT(ParsePB(data), StrEq(R"(1: "Hello world")"
                                   "\n"
                                   R"(1: "Hello world")"
                                   "\n"
                                   R"(1: "Hello world")"));
}

TEST(ParsePB, LongStringTruncation) {
  std::string_view data = CreateStringView<char>(
      "\x00\x00\x00\x00\x49\x0A\x47This is a long string. It is so long that is expected to get "
      "truncated.");
  EXPECT_THAT(ParsePB(data, /*is_gzipped*/ false, /*str_field_truncation_len*/ 32),
              StrEq(R"(1: "This is a long string. It is so ...<truncated>...")"));
}

// Tests that ParsePB() can parse partial serialized message, and produces partial text format.
TEST(ParsePb, ParsingPartialMessage) {
  const std::string program_protobuf = R"proto(
                                       b: true
                                       i32: 100
                                       i64: 200
                                       f: 1.2345
                                       bs: "1234"
                                       str: "string"
                                       )proto";
  MultiFieldsMessage multi_fields_msg;

  ASSERT_TRUE(TextFormat::ParseFromString(program_protobuf, &multi_fields_msg));
  std::string data = multi_fields_msg.SerializeAsString();

  ASSERT_GT(data.size(), 10);
  // Truncate data to 100 bytes.
  data.resize(10);

  // gRPC payload has 5 bytes header: <compression 1 byte> + <size 4 bytes>.
  std::string_view grpc_payload_header = CreateStringView<char>("\x00\x00\x00\x00\x0A");
  std::string message = absl::StrCat(grpc_payload_header, data);

  EXPECT_THAT(ParsePB(message), StrEq(R"(1: 1
2: 100
3: 200
4: 0x00000419)"));
}

// Tests that result of parsing when the gunzip fails and other failures.
TEST(ParsePbTest, ParsingInvalidGZippedData) {
  std::string_view data = CreateStringView<char>("\x01\x00\x00\x00\x01\x0A");
  EXPECT_THAT(ParsePB(data, /*is_gzipped*/ false), StrEq("<Non-gzip decompression not supported>"));
  EXPECT_THAT(ParsePB(data, /*is_gzipped*/ true), StrEq("<GZip decompression is disabled>"));
  FLAGS_socket_tracer_enable_http2_gzip = true;
  EXPECT_THAT(ParsePB(data, /*is_gzipped*/ true), StrEq("<Failed to gunzip data>"));
  FLAGS_socket_tracer_enable_http2_gzip = false;
}

// Tests that request & response bodies are unchanged if the grpc-encoding has a value that is not
// gzip.
TEST(ParseReqRespBodyTest, BodyUnchangedForUnsupportedCompressionAlgo) {
  protocols::http2::Stream http2_stream;
  http2_stream.send.mutable_data()->assign("sent message");
  http2_stream.send.mutable_headers()->insert(std::make_pair("grpc-encoding", "7zip"));
  http2_stream.recv.mutable_data()->assign("recv message");
  ParseReqRespBody(&http2_stream);
  EXPECT_THAT(http2_stream.send.data(), StrEq("sent message"));
  EXPECT_THAT(http2_stream.recv.data(), StrEq("recv message"));
}

}  // namespace grpc
}  // namespace stirling
}  // namespace px
