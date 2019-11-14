#include "src/stirling/http2/grpc.h"

#include "src/common/testing/testing.h"
#include "src/stirling/http2/testing/proto/greet.pb.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::pl::stirling::http2::testing::HelloRequest;
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

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
