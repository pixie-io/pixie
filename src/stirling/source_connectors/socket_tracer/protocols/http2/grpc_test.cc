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
  HelloRequest req;
  {
    std::string text;
    EXPECT_NOT_OK(PBWireToText("", PBTextFormat::kText, &req, &text));
  }
  {
    std::string text;
    EXPECT_NOT_OK(PBWireToText("a", PBTextFormat::kText, &req, &text));
  }
  {
    std::string text;
    EXPECT_NOT_OK(PBWireToText("aa", PBTextFormat::kText, &req, &text));
  }
  {
    std::string text;
    EXPECT_NOT_OK(PBWireToText("aaa", PBTextFormat::kText, &req, &text));
  }
  {
    std::string text;
    EXPECT_NOT_OK(PBWireToText("aaaa", PBTextFormat::kText, &req, &text));
  }
  {
    std::string text;
    EXPECT_NOT_OK(PBWireToText("aaaaa", PBTextFormat::kText, &req, &text));
  }
  {
    std::string text;
    std::string_view s = CreateStringView<char>("\x00\x00\x00\x00\x00");
    EXPECT_OK(PBWireToText(s, PBTextFormat::kText, &req, &text));
    EXPECT_OK(PBWireToText(s, PBTextFormat::kJSON, &req, &text));
  }
  {
    std::string text;
    std::string_view data = "test";
    std::string_view header = CreateStringView<char>("\x00\x00\x00\x00\x04");
    std::string invalid_serialized_pb = absl::StrCat(header, data);
    EXPECT_NOT_OK(PBWireToText(invalid_serialized_pb, PBTextFormat::kText, &req, &text));
    EXPECT_NOT_OK(PBWireToText(invalid_serialized_pb, PBTextFormat::kJSON, &req, &text));
  }
  req.set_name("pixielabs");
  std::string s = PackGRPCMsg(req.SerializeAsString());
  {
    std::string text;
    EXPECT_OK(PBWireToText(s, PBTextFormat::kJSON, &req, &text));
    EXPECT_THAT(text, StrEq(R"json({"name":"pixielabs"})json"));
  }
  {
    std::string text;
    EXPECT_OK(PBWireToText(s, PBTextFormat::kText, &req, &text));
    EXPECT_THAT(text, StrEq("name: \"pixielabs\"\n"));
  }
}

TEST(ParsePB, Basic) {
  HelloRequest req;
  req.set_name("pixielabs");
  std::string s = PackGRPCMsg(req.SerializeAsString());

  HelloRequest pb;
  EXPECT_EQ(ParsePB(s, &pb), "name: \"pixielabs\"");
  EXPECT_EQ(ParsePB(s, nullptr), "1 {\n  14: 105\n  15: 105\n  12: 0x7362616c\n}");
}

TEST(ParsePB, Nullptr) {
  HelloRequest req;
  req.set_name("pixielabs");
  std::string s = PackGRPCMsg(req.SerializeAsString());

  EXPECT_EQ(ParsePB(s, nullptr), "1 {\n  14: 105\n  15: 105\n  12: 0x7362616c\n}");
}

TEST(ParsePB, ErrorMessage) {
  HelloRequest req;
  std::string text(kGRPCMessageHeaderSizeInBytes + 1, 0x0A);
  EXPECT_THAT(ParsePB(text, &req), HasSubstr("original data in hex format: "
                                             R"(\x0A\x0A\x0A\x0A\x0A\x0A)"));
}

TEST(ParsePB, MultipleGRPCLengthPrefixedMessages) {
  std::string_view data = CreateStringView<char>(
      "\x00\x00\x00\x00\x0D\x0A\x0B\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64"
      "\x00\x00\x00\x00\x0D\x0A\x0B\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64"
      "\x00\x00\x00\x00\x0D\x0A\x0B\x48\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64");
  HelloReply pb;
  EXPECT_EQ(
      "message: \"Hello world\"\n"
      "message: \"Hello world\"\n"
      "message: \"Hello world\"",
      ParsePB(data, &pb));
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
