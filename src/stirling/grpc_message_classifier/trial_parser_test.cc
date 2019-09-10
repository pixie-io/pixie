#include "src/stirling/grpc_message_classifier/trial_parser.h"

#include <string>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace grpc {

using google::protobuf::FileDescriptorSet;
using google::protobuf::TextFormat;
using pl::grpc::ServiceDescriptorDatabase;

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

class ParseAsTest : public ::testing::Test {
 protected:
  void SetUp() {
    FileDescriptorSet fd_set;
    ASSERT_TRUE(TextFormat::ParseFromString(kTestProtoBuf, fd_set.add_file()));
    db_ = std::make_unique<ServiceDescriptorDatabase>(fd_set);
  }

  std::unique_ptr<ServiceDescriptorDatabase> db_;
  StatusOr<std::unique_ptr<google::protobuf::Message>> message_;

  const ParseAsOpts kAllowUnknownFields = {.allow_unknown_fields = true,
                                           .allow_repeated_opt_fields = false};
  const ParseAsOpts kAllowRepeatedOptFields = {.allow_unknown_fields = false,
                                               .allow_repeated_opt_fields = true};
};

TEST_F(ParseAsTest, ValidMessage) {
  // Message with the string '581a554f-33' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ =
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage, kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ =
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage, kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());
}

TEST_F(ParseAsTest, EmptyMessage) {
  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "");
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "", kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "", kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());
}

TEST_F(ParseAsTest, InvalidMessageType) {
  // Message with the string '581a554f-33' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  StatusOr<std::unique_ptr<google::protobuf::Message>> message_;

  message_ = ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage);
  EXPECT_FALSE(message_.ok());

  message_ = ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage, kAllowUnknownFields);
  EXPECT_FALSE(message_.ok());

  message_ = ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage, kAllowRepeatedOptFields);
  EXPECT_FALSE(message_.ok());
}

TEST_F(ParseAsTest, WrongMessageLength) {
  // Message with incorrect protobuf length (not parseable).
  const std::string kMessageWrongLength = "\x0a\x06\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  StatusOr<std::unique_ptr<google::protobuf::Message>> message_;

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ =
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength, kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength,
                     kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());
}

TEST_F(ParseAsTest, WrongWireType) {
  // Message with incorrect wire_type (field 1 as a varint instead of string).
  const std::string kMessageWrongWireType = "\x08\x01";

  StatusOr<std::unique_ptr<google::protobuf::Message>> message_;

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  // NOTE: This might be unexpected, because allowing unknown fields
  //       actually allows a message with wrong wire type to be parsed.
  // TODO(oazizi): Can we fix/change this behavior?
  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType,
                     kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType,
                     kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());
}

TEST_F(ParseAsTest, MessageWithExtraField) {
  // Valid message with extra field number 2.
  const std::string kMessageWithExtraField =
      absl::StrCat("\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33", "\x10\x01");

  StatusOr<std::unique_ptr<google::protobuf::Message>> message_;

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  // Note that this is a different kind of unknown field than the rest.
  // Technically, this one is a valid message_, while duplicate field number and wrong wire type are
  // not.
  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField,
                     kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField,
                     kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());
}

TEST_F(ParseAsTest, MessageWithConflictingFieldNum) {
  // Message with repeated field number (field number 1 specified twice, once as string, once as
  // varint).
  const std::string kMessageWithConflictingFieldNum1 =
      "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33\x08\x01";

  // Message with repeated field number (like above, but with order flipped).
  const std::string kMessageWithConflictingFieldNum2 =
      "\x08\x01\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum1);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum1,
                     kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum1,
                     kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum2);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum2,
                     kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum2,
                     kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());
}

TEST_F(ParseAsTest, MessageWithDuplicateFieldNum) {
  // Message with the string '581a' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x04\x35\x38\x31\x61";
  const std::string kMessageWithDuplicateFieldNum = absl::StrCat(kValidMessage, kValidMessage);

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum,
                     kAllowUnknownFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_EQ(nullptr, message_.ConsumeValueOrDie());

  message_ = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum,
                     kAllowRepeatedOptFields);
  ASSERT_TRUE(message_.ok());
  EXPECT_NE(nullptr, message_.ConsumeValueOrDie());
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
