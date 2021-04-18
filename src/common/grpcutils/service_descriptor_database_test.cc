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

#include "src/common/grpcutils/service_descriptor_database.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <string>

#include "src/common/testing/testing.h"

namespace px {
namespace grpc {

using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::px::testing::proto::EqualsProto;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;

const char kTestProtoBuf[] = R"proto(
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
      service {
        name: "CheckoutAgainService"
        method {
          name: "PlaceOrderAgain"
          input_type: "PlaceOrderRequest"
          output_type: "PlaceOrderResponse"
        }
      }
  )proto";

class ServiceDescriptorDatabaseTest : public ::testing::Test {
 protected:
  void SetUp() {
    FileDescriptorSet fd_set;
    ASSERT_TRUE(TextFormat::ParseFromString(kTestProtoBuf, fd_set.add_file()));

    db_ = std::make_unique<ServiceDescriptorDatabase>(fd_set);
  }

  std::unique_ptr<ServiceDescriptorDatabase> db_;
};

TEST_F(ServiceDescriptorDatabaseTest, GetInputOutput) {
  MethodInputOutput in_out = db_->GetMethodInputOutput("hipstershop.CheckoutService.PlaceOrder");
  ASSERT_NE(nullptr, in_out.input);
  ASSERT_NE(nullptr, in_out.output);

  const char kExpectedReqInText[] = R"proto(user_id: "pixielabs")proto";
  const char kExpectedRespInText[] = R"proto(ok: true)proto";

  // Verify dynamic message can parse text format protobuf.
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedReqInText, in_out.input.get()));
  EXPECT_THAT(*in_out.input, EqualsProto(kExpectedReqInText));

  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedRespInText, in_out.output.get()));
  EXPECT_THAT(*in_out.output, EqualsProto(kExpectedRespInText));
}

TEST_F(ServiceDescriptorDatabaseTest, GetMessage) {
  std::unique_ptr<Message> msg = db_->GetMessage("hipstershop.PlaceOrderRequest");
  ASSERT_NE(nullptr, msg);

  constexpr char kExpectedReqInText[] = R"proto(user_id: "pixielabs")proto";
  ASSERT_TRUE(TextFormat::ParseFromString(kExpectedReqInText, msg.get()));
  EXPECT_THAT(*msg, EqualsProto(kExpectedReqInText));
}

TEST_F(ServiceDescriptorDatabaseTest, AllServices) {
  std::vector<google::protobuf::ServiceDescriptorProto> services = db_->AllServices();

  ASSERT_EQ(2, services.size());
  EXPECT_EQ("CheckoutService", services[0].name());
  EXPECT_EQ("CheckoutAgainService", services[1].name());
}

}  // namespace grpc
}  // namespace px
