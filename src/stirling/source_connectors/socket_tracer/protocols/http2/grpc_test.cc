#include "src/stirling/source_connectors/socket_tracer/protocols/http2/grpc.h"

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/testing/proto/greet.pb.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::pl::stirling::protocols::http2::testing::HelloReply;
using ::pl::stirling::protocols::http2::testing::HelloRequest;
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
  EXPECT_THAT(ParsePB("aaaaa"), StrEq("<Failed to parse protobuf>"));
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

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
