#include "src/shared/grpc/service_descriptor_database.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <string>

#include "demos/applications/hipster_shop/proto/demo.pb.h"
#include "demos/applications/hipster_shop/reflection.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace grpc {

using ::demos::hipster_shop::GetFileDescriptorSet;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::pl::testing::proto::EqualsProto;

TEST(ServiceDescriptorDatabaseTest, GetInputOutput) {
  ServiceDescriptorDatabase db(GetFileDescriptorSet());
  MethodInputOutput in_out = db.GetMethodInputOutput("hipstershop.CheckoutService.PlaceOrder");
  EXPECT_NE(nullptr, in_out.input);
  EXPECT_NE(nullptr, in_out.output);

  hipstershop::PlaceOrderRequest expected_req;
  expected_req.set_user_id("pixielabs");
  expected_req.set_user_currency("libra");
  expected_req.set_email("admin@pixielabs.ai");
  expected_req.mutable_address()->set_street_address("300 Br");
  expected_req.mutable_credit_card()->set_credit_card_number("0123 4567 8901 2345");

  std::string expected_text_req;
  EXPECT_TRUE(TextFormat::PrintToString(expected_req, &expected_text_req));

  // Verify dynamic message can parse text format protobuf.
  EXPECT_TRUE(TextFormat::ParseFromString(expected_text_req, in_out.input.get()));
  EXPECT_THAT(*in_out.input, EqualsProto(expected_text_req));

  in_out.input->Clear();
  std::string expected_binary_req = expected_req.SerializeAsString();

  // Verify dynamic message can parse binary format protobuf.
  EXPECT_TRUE(in_out.input->ParseFromString(expected_binary_req));
  EXPECT_THAT(*in_out.input, EqualsProto(expected_text_req));

  // Have different descriptors.
  EXPECT_NE(in_out.input->GetDescriptor(), expected_req.GetDescriptor());

  hipstershop::PlaceOrderResponse expected_resp;
  expected_resp.mutable_order()->set_order_id("12345");
  expected_resp.mutable_order()->mutable_shipping_cost()->set_currency_code("usd");
  expected_resp.mutable_order()->mutable_shipping_address()->set_street_address("300 Br");
  expected_resp.mutable_order()->add_items()->mutable_item()->set_product_id("product_123");

  std::string expected_text_resp;
  EXPECT_TRUE(TextFormat::PrintToString(expected_resp, &expected_text_resp));

  EXPECT_TRUE(TextFormat::ParseFromString(expected_text_resp, in_out.output.get()));
  EXPECT_THAT(*in_out.output, EqualsProto(expected_text_resp));

  in_out.output->Clear();
  std::string expected_binary_resp = expected_resp.SerializeAsString();

  EXPECT_TRUE(in_out.output->ParseFromString(expected_binary_resp));
  EXPECT_THAT(*in_out.output, EqualsProto(expected_text_resp));

  EXPECT_NE(in_out.output->GetDescriptor(), expected_resp.GetDescriptor());
}

}  // namespace grpc
}  // namespace pl
