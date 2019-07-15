#include "src/common/grpcutils/service_descriptor_database.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <string>

#include "src/common/testing/testing.h"

namespace pl {
namespace grpc {

using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::pl::testing::proto::EqualsProto;

TEST(ServiceDescriptorDatabaseTest, GetInputOutput) {
  FileDescriptorSet fd_set;

  TextFormat::ParseFromString(R"proto(
      name: "demo.proto"
      package: "hipstershop"
      message_type {
        name: "PlaceOrderRequest"
        field {
          name: "user_id"
          number: 1
          type: TYPE_STRING
        }
      }
      message_type {
        name: "PlaceOrderResponse"
        field {
          name: "ok"
          number: 1
          type: TYPE_BOOL
        }
      }
      service {
        name: "CheckoutService"
        method {
          name: "PlaceOrder"
          input_type: "PlaceOrderRequest"
          output_type: "PlaceOrderResponse"
        }
      }
  )proto",
                              fd_set.add_file());
  ServiceDescriptorDatabase db(fd_set);
  MethodInputOutput in_out = db.GetMethodInputOutput("hipstershop.CheckoutService.PlaceOrder");
  EXPECT_NE(nullptr, in_out.input);
  EXPECT_NE(nullptr, in_out.output);

  const char kExpectedReqInText[] = R"proto(user_id: "pixielabs")proto";
  const char kExpectedRespInText[] = R"proto(ok: true)proto";

  // Verify dynamic message can parse text format protobuf.
  EXPECT_TRUE(TextFormat::ParseFromString(kExpectedReqInText, in_out.input.get()));
  EXPECT_THAT(*in_out.input, EqualsProto(kExpectedReqInText));

  EXPECT_TRUE(TextFormat::ParseFromString(kExpectedRespInText, in_out.output.get()));
  EXPECT_THAT(*in_out.output, EqualsProto(kExpectedRespInText));
}

}  // namespace grpc
}  // namespace pl
