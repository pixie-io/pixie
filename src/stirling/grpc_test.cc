#include "src/stirling/grpc.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/testing/proto/greet.pb.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::pl::stirling::testing::HelloRequest;
using ::testing::StrEq;

TEST(ParseProtobufTest, VariousErrors) {
  HelloRequest req;
  std::string json;
  for (size_t i = 0; i < kGRPCMessageHeaderSizeInBytes; ++i) {
    std::string s(i, 'a');
    EXPECT_NOT_OK(ParseProtobuf(s, &req, &json));
  }
  std::string s(kGRPCMessageHeaderSizeInBytes, 'a');
  EXPECT_OK(ParseProtobuf(s, &req, &json));

  EXPECT_NOT_OK(ParseProtobuf(s, nullptr, &json));

  std::string invalid_serialized_pb = s + "not valid";
  EXPECT_NOT_OK(ParseProtobuf(invalid_serialized_pb, &req, &json));

  req.set_name("pixielabs");
  std::string valid_serialized_pb = s + req.SerializeAsString();
  EXPECT_OK(ParseProtobuf(valid_serialized_pb, &req, &json));
  EXPECT_THAT(json, StrEq(R"json({}{"name":"pixielabs"})json"));
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
