#include "src/stirling/http2/grpc.h"

#include "src/common/testing/testing.h"
#include "src/stirling/http2/testing/proto/greet.pb.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::pl::stirling::http2::testing::HelloRequest;
using ::testing::HasSubstr;
using ::testing::StrEq;

TEST(ParseProtobufTest, VariousErrors) {
  HelloRequest req;
  std::string text;
  for (size_t i = 0; i < kGRPCMessageHeaderSizeInBytes; ++i) {
    std::string s(i, 'a');
    EXPECT_NOT_OK(PBWireToText(s, PBTextFormat::kText, &req, &text));
  }
  std::string s(kGRPCMessageHeaderSizeInBytes, 'a');
  EXPECT_OK(PBWireToText(s, PBTextFormat::kText, &req, &text));
  EXPECT_OK(PBWireToText(s, PBTextFormat::kJSON, &req, &text));

  std::string invalid_serialized_pb = s + "not valid";
  EXPECT_NOT_OK(PBWireToText(invalid_serialized_pb, PBTextFormat::kText, &req, &text));
  EXPECT_NOT_OK(PBWireToText(invalid_serialized_pb, PBTextFormat::kJSON, &req, &text));

  req.set_name("pixielabs");
  std::string valid_serialized_pb = s + req.SerializeAsString();
  EXPECT_OK(PBWireToText(valid_serialized_pb, PBTextFormat::kJSON, &req, &text));
  EXPECT_THAT(text, StrEq(R"json({}{"name":"pixielabs"})json"));

  EXPECT_OK(PBWireToText(valid_serialized_pb, PBTextFormat::kText, &req, &text));
  EXPECT_THAT(text, StrEq("name: \"pixielabs\"\n"));
}

TEST(ParsePB, Basic) {
  std::string s;
  {
    HelloRequest req;
    req.set_name("pixielabs");
    s = std::string(kGRPCMessageHeaderSizeInBytes, 'a') + req.SerializeAsString();
  }

  HelloRequest pb;
  EXPECT_EQ(ParsePB(s, &pb), "name: \"pixielabs\"");
  EXPECT_EQ(ParsePB(s, nullptr), "1 {\n  14: 105\n  15: 105\n  12: 0x7362616c\n}");
}

TEST(ParsePB, Nullptr) {
  std::string s;
  {
    HelloRequest req;
    req.set_name("pixielabs");
    s = std::string(kGRPCMessageHeaderSizeInBytes, 'a') + req.SerializeAsString();
  }

  HelloRequest pb;
  EXPECT_EQ(ParsePB(s, nullptr), "1 {\n  14: 105\n  15: 105\n  12: 0x7362616c\n}");
}

TEST(ParsePB, ErrorMessage) {
  HelloRequest req;
  std::string text(kGRPCMessageHeaderSizeInBytes + 1, 0x0A);
  EXPECT_THAT(ParsePB(text, &req), HasSubstr("original data in hex format: 0A0A0A0A0A0A"));
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
