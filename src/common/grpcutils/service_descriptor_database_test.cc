#include "src/common/grpcutils/service_descriptor_database.h"

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <string>

#include "src/common/testing/testing.h"

namespace pl {
namespace grpc {

using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using ::pl::testing::proto::EqualsProto;
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

TEST_F(ServiceDescriptorDatabaseTest, ParseAs) {
  // Message with the string '581a554f-33' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  // Message with incorrect protobuf length (not parseable).
  const std::string kMessageWrongLength = "\x0a\x06\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  // Message with incorrect wire_type (1 as a varint).
  const std::string kMessageWrongWireType = "\x08\x01";

  // Message with extra field.
  const std::string kMessageWithExtraField =
      "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33\x10\x01";

  // Message with repeated field number (field number 1 specified twice, once as string, once as
  // varint).
  const std::string kMessageWithDuplicateFieldNum =
      "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33\x08\x01";

  // Message with repeated field number (like above, but with order flipped).
  const std::string kMessageWithDuplicateFieldNum2 =
      "\x08\x01\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  StatusOr<std::unique_ptr<google::protobuf::Message>> message;

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage,
                    /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage);
  EXPECT_FALSE(message.ok());

  message =
      ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage, /* allow_unknown_fields */ true);
  EXPECT_FALSE(message.ok());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength);
  ASSERT_TRUE(message.ok());
  EXPECT_EQ(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength,
                    /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_EQ(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType);
  ASSERT_TRUE(message.ok());
  EXPECT_EQ(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType,
                    /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField);
  ASSERT_TRUE(message.ok());
  EXPECT_EQ(nullptr, message.ConsumeValueOrDie());

  // Note that this is a different kind of unknown field than the rest.
  // Technically, this one is a valid message, while duplicate field number and wrong wire type are
  // not.
  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField,
                    /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum);
  ASSERT_TRUE(message.ok());
  EXPECT_EQ(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum,
                    /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum2);
  ASSERT_TRUE(message.ok());
  EXPECT_EQ(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum2,
                    /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "");
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());

  message =
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "", /* allow_unknown_fields */ true);
  ASSERT_TRUE(message.ok());
  EXPECT_NE(nullptr, message.ConsumeValueOrDie());
}

}  // namespace grpc
}  // namespace pl
