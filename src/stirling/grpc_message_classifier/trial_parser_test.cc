#include "src/stirling/grpc_message_classifier/trial_parser.h"

#include <fstream>
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

const ParseAsOpts kAllowUnknownFields = {.allow_unknown_fields = true,
                                         .allow_repeated_opt_fields = false};

const ParseAsOpts kAllowRepeatedOptFields = {.allow_unknown_fields = false,
                                             .allow_repeated_opt_fields = true};

class ParseAsTest : public ::testing::Test {
 protected:
  void SetUp() {
    FileDescriptorSet fd_set;
    ASSERT_TRUE(TextFormat::ParseFromString(kTestProtoBuf, fd_set.add_file()));
    db_ = std::make_unique<ServiceDescriptorDatabase>(fd_set);
  }

  std::unique_ptr<ServiceDescriptorDatabase> db_;
};

TEST_F(ParseAsTest, ValidMessage) {
  // Message with the string '581a554f-33' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage), nullptr);
  EXPECT_OK_AND_NE(
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage, kAllowUnknownFields),
      nullptr);
  EXPECT_OK_AND_NE(
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kValidMessage, kAllowRepeatedOptFields),
      nullptr);
}

TEST_F(ParseAsTest, EmptyMessage) {
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", ""), nullptr);
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "", kAllowUnknownFields),
                   nullptr);
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", "", kAllowRepeatedOptFields),
                   nullptr);
}

TEST_F(ParseAsTest, InvalidMessageType) {
  // Message with the string '581a554f-33' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  EXPECT_NOT_OK(ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage));
  EXPECT_NOT_OK(ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage, kAllowUnknownFields));
  EXPECT_NOT_OK(
      ParseAs(db_.get(), "hipstershop.FakeRequest", kValidMessage, kAllowRepeatedOptFields));
}

TEST_F(ParseAsTest, WrongMessageLength) {
  // Message with incorrect protobuf length (not parseable).
  const std::string kMessageWrongLength = "\x0a\x06\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33";

  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength),
                   nullptr);
  EXPECT_OK_AND_EQ(
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength, kAllowUnknownFields),
      nullptr);
  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongLength,
                           kAllowRepeatedOptFields),
                   nullptr);
}

TEST_F(ParseAsTest, WrongWireType) {
  // Message with incorrect wire_type (field 1 as a varint instead of string).
  const std::string kMessageWrongWireType = "\x08\x01";

  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType),
                   nullptr);
  // NOTE: This might be unexpected, because allowing unknown fields
  //       actually allows a message with wrong wire type to be parsed.
  // TODO(oazizi): Can we fix/change this behavior?
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType,
                           kAllowUnknownFields),
                   nullptr);
  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWrongWireType,
                           kAllowRepeatedOptFields),
                   nullptr);
}

TEST_F(ParseAsTest, MessageWithExtraField) {
  // Valid message with extra field number 2.
  const std::string kMessageWithExtraField =
      absl::StrCat("\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33", "\x10\x01");

  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField),
                   nullptr);
  // Note that this is a different kind of unknown field than the rest.
  // Technically, this one is a valid message_, while duplicate field number and wrong wire type are
  // not.
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField,
                           kAllowUnknownFields),
                   nullptr);
  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithExtraField,
                           kAllowRepeatedOptFields),
                   nullptr);
}

TEST_F(ParseAsTest, MessageWithConflictingFieldNum) {
  // Message with repeated field number (field number 1 specified twice, once as string, once as
  // varint).
  const std::string kMessageWithConflictingFieldNum1 =
      absl::StrCat("\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33", "\x08\x01");

  // Message with repeated field number (like above, but with order flipped).
  const std::string kMessageWithConflictingFieldNum2 =
      absl::StrCat("\x08\x01", "\x0a\x0b\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x33");

  EXPECT_OK_AND_EQ(
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum1),
      nullptr);
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest",
                           kMessageWithConflictingFieldNum1, kAllowUnknownFields),
                   nullptr);
  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest",
                           kMessageWithConflictingFieldNum1, kAllowRepeatedOptFields),
                   nullptr);
  EXPECT_OK_AND_EQ(
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithConflictingFieldNum2),
      nullptr);
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest",
                           kMessageWithConflictingFieldNum2, kAllowUnknownFields),
                   nullptr);
  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest",
                           kMessageWithConflictingFieldNum2, kAllowRepeatedOptFields),
                   nullptr);
}

TEST_F(ParseAsTest, MessageWithDuplicateFieldNum) {
  // Message with the string '581a' in protobuf wire format.
  const std::string kValidMessage = "\x0a\x04\x35\x38\x31\x61";
  const std::string kMessageWithDuplicateFieldNum = absl::StrCat(kValidMessage, kValidMessage);

  EXPECT_OK_AND_EQ(
      ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", kMessageWithDuplicateFieldNum), nullptr);
  EXPECT_OK_AND_EQ(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest",
                           kMessageWithDuplicateFieldNum, kAllowUnknownFields),
                   nullptr);
  EXPECT_OK_AND_NE(ParseAs(db_.get(), "hipstershop.PlaceOrderRequest",
                           kMessageWithDuplicateFieldNum, kAllowRepeatedOptFields),
                   nullptr);
}

TEST_F(ParseAsTest, NonUTFStrings) {
  constexpr int kAsciiChars = 256;
  constexpr int kStrLength = 64;

  // This Protobuf header represents a string wire type with a length of 64.
  std::string msg = "\x0a\x40";
  msg.resize(msg.length() + kStrLength);

  // Make the string have every possible UTF-8 character.
  // But because the string length is 64, use multiple iterations.
  //
  // On iteration 0: \x00\x01\x02...\x3f
  // On iteration 1: \x40\x41\x42...\x7f
  // On iteration 2: \x80\x81\x82...\xbf
  // On iteration 3: \xc0\x41\x42...\xff
  for (int k = 0; k < kAsciiChars / kStrLength; k++) {
    for (int i = 0; i < kStrLength; ++i) {
      msg[2 + i] = static_cast<char>(k * 64 + i);
    }

    StatusOr<std::unique_ptr<google::protobuf::Message>> message;

    message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", msg);
    EXPECT_OK_AND_NE(message, nullptr);
    EXPECT_EQ(message.ValueOrDie()->ByteSize(), 66);

    message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", msg, kAllowUnknownFields);
    EXPECT_OK_AND_NE(message, nullptr);
    EXPECT_EQ(message.ValueOrDie()->ByteSize(), 66);

    message = ParseAs(db_.get(), "hipstershop.PlaceOrderRequest", msg, kAllowRepeatedOptFields);
    EXPECT_OK_AND_NE(message, nullptr);
    EXPECT_EQ(message.ValueOrDie()->ByteSize(), 66);
  }
}

// This test takes a captured AgentQueryResultRequest protobuf message
// and tries to parse it against two versions of the AgentQueryResultRequest definition.
// In one version the StringColumn definition uses type 'string'.
// In the second version the StringColumn definition uses type 'bytes'.
//
// While string and bytes are treated by the protobuf library in mostly the same way,
// this test shows that there is a material difference:
// The message turns out to be parsable with type bytes, but not as type string.
//
// Note that simpler messages typically can parse as either kind, which
// is why this large message is used here.
//
// It is left as future work to better understand why this particular message
// is parseable with a bytes field but not a string field.
TEST(ParseAsStress, BytesVsString) {
  // Get a captured AgentQueryResultRequest message.
  std::string message_filename =
      testing::TestFilePath("src/stirling/grpc_message_classifier/testdata/message");
  std::string message_ascii = pl::FileContentsOrDie(message_filename);
  auto status_or_bytes = AsciiHexToBytes<std::string>(message_ascii, {':'});
  ASSERT_OK(status_or_bytes);
  std::string message = status_or_bytes.ValueOrDie();

  const std::string kMessageType =
      "pl.vizier.services.query_broker.querybrokerpb.AgentQueryResultRequest";

  // The FileDescriptorSets below are protobuf representations of Pixie proto definitions.
  // They are generated via the following command:
  // protoc -o pixie.fds --include_imports
  //         src/vizier/services/query_broker/querybrokerpb/service.proto
  //         src/vizier/services/metadata/metadatapb/service.proto

  // Version with string parses correctly.
  {
    // This FDS uses string as the StringColumn representation.
    std::string fds_filename =
        testing::TestFilePath("src/stirling/grpc_message_classifier/testdata/pixie-string.fds");

    // TODO(oazizi): Replace lines below with changes from https://phab.pixielabs.ai/D1780.
    std::ifstream fds_file(fds_filename);
    ASSERT_TRUE(fds_file.is_open());
    ::google::protobuf::FileDescriptorSet file_descriptor_set;
    file_descriptor_set.ParseFromIstream(&fds_file);
    ::pl::grpc::ServiceDescriptorDatabase db(file_descriptor_set);

    EXPECT_OK_AND_EQ(ParseAs(&db, kMessageType, message), nullptr);
    EXPECT_OK_AND_EQ(ParseAs(&db, kMessageType, message, kAllowUnknownFields), nullptr);
    EXPECT_OK_AND_EQ(ParseAs(&db, kMessageType, message, kAllowRepeatedOptFields), nullptr);
  }

  // Version with bytes parses correctly.
  {
    // This FDS uses string as the StringColumn representation.
    std::string fds_filename =
        testing::TestFilePath("src/stirling/grpc_message_classifier/testdata/pixie-bytes.fds");

    // TODO(oazizi): Replace lines below with changes from https://phab.pixielabs.ai/D1780.
    std::ifstream fds_file(fds_filename);
    ASSERT_TRUE(fds_file.is_open());
    ::google::protobuf::FileDescriptorSet file_descriptor_set;
    file_descriptor_set.ParseFromIstream(&fds_file);
    ::pl::grpc::ServiceDescriptorDatabase db(file_descriptor_set);

    StatusOr<std::unique_ptr<google::protobuf::Message>> parsed_message;

    EXPECT_OK_AND_NE(ParseAs(&db, kMessageType, message), nullptr);
    EXPECT_OK_AND_NE(ParseAs(&db, kMessageType, message, kAllowUnknownFields), nullptr);
    EXPECT_OK_AND_NE(ParseAs(&db, kMessageType, message, kAllowRepeatedOptFields), nullptr);
  }
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
