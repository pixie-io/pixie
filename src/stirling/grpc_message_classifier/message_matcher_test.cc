#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "demos/applications/hipster_shop/reflection.h"
#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/common/testing/status.h"
#include "src/stirling/grpc_message_classifier/message_matcher.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::ServiceDescriptorProto;

using ::pl::grpc::ServiceDescriptorDatabase;

using ::pl::stirling::grpc::MessageDirection;
using ::pl::stirling::grpc::MessageGroupTypeClassifier;

using ::pl::stirling::http2::IndexedHeaderField;
using ::pl::stirling::http2::LiteralHeaderField;
using ::pl::testing::status::IsOKAndHolds;

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::SizeIs;

class MessageMatcherTest : public ::testing::Test {
 protected:
  MessageMatcherTest() : desc_db_(demos::hipster_shop::GetFileDescriptorSet()) {}

  ::pl::grpc::ServiceDescriptorDatabase desc_db_;
};

MATCHER_P(HasName, name, "") { return arg.name() == name; }
MATCHER_P(HasInputType, input_type, "") { return arg.input_type() == input_type; }
MATCHER_P(HasOutputType, output_type, "") { return arg.output_type() == output_type; }

TEST_F(MessageMatcherTest, InitialCandidatesTest) {
  {
    MessageGroupTypeClassifier msg_matcher(&desc_db_, {}, kDefaultParseAsOpts);

    std::set<std::string> candidate_message_types;
    std::set<::google::protobuf::MethodDescriptorProto> candidate_rpcs;

    // There are 15 RPCs, with 14 unique input message types,
    // and 13 unique output message types, due to some sharing of types across RPCs.
    EXPECT_EQ(msg_matcher.MatchingRPCs().size(), 15);
    EXPECT_EQ(msg_matcher.MatchingMessageTypes(MessageDirection::kRequest).size(), 14);
    EXPECT_EQ(msg_matcher.MatchingMessageTypes(MessageDirection::kResponse).size(), 13);
  }

  {
    MessageGroupTypeClassifier msg_matcher(&desc_db_, {"CartService"}, kDefaultParseAsOpts);

    std::set<std::string> candidate_message_types;
    std::set<::google::protobuf::MethodDescriptorProto> candidate_rpcs;

    // There are 3 RPCs, each with different input message types,
    // but two of which share an output message type.
    EXPECT_EQ(msg_matcher.MatchingRPCs().size(), 3);
    EXPECT_EQ(msg_matcher.MatchingMessageTypes(MessageDirection::kRequest).size(), 3);
    EXPECT_EQ(msg_matcher.MatchingMessageTypes(MessageDirection::kResponse).size(), 2);
  }
}

TEST_F(MessageMatcherTest, AddSingleMessage) {
  {
    // A hipstershop.EmailService/SendOrderConfirmation request.
    std::string message =
        "\x0a\x13\x73\x6f\x6d\x65\x6f\x6e\x65\x40\x65\x78\x61\x6d\x70\x6c\x65\x2e\x63\x6f\x6d\x12"
        "\xee\x01\x0a\x24\x61\x34\x31\x32\x31\x65\x66\x61\x2d\x63\x33\x64\x32\x2d\x31\x31\x65\x39"
        "\x2d\x38\x34\x32\x33\x2d\x30\x61\x35\x38\x30\x61\x32\x34\x30\x34\x38\x38\x12\x12\x4a\x4c"
        "\x2d\x34\x34\x31\x39\x32\x2d\x32\x32\x34\x34\x35\x31\x30\x35\x39\x1a\x0d\x0a\x03\x45\x55"
        "\x52\x10\x0f\x18\xaa\xf6\xb4\xcf\x01\x22\x40\x0a\x19\x31\x36\x30\x30\x20\x41\x6d\x70\x68"
        "\x69\x74\x68\x65\x61\x74\x72\x65\x20\x50\x61\x72\x6b\x77\x61\x79\x12\x0d\x4d\x6f\x75\x6e"
        "\x74\x61\x69\x6e\x20\x56\x69\x65\x77\x1a\x02\x43\x41\x22\x0d\x55\x6e\x69\x74\x65\x64\x20"
        "\x53\x74\x61\x74\x65\x73\x28\xe7\x56\x2a\x1f\x0a\x0e\x0a\x0a\x30\x50\x55\x4b\x36\x56\x36"
        "\x45\x56\x30\x10\x04\x12\x0d\x0a\x03\x45\x55\x52\x10\x3a\x18\xfb\x9e\xfa\xe2\x02\x2a\x1f"
        "\x0a\x0e\x0a\x0a\x4c\x53\x34\x50\x53\x58\x55\x4e\x55\x4d\x10\x01\x12\x0d\x0a\x03\x45\x55"
        "\x52\x10\x15\x18\xdb\x92\xa7\x87\x03\x2a\x1f\x0a\x0e\x0a\x0a\x39\x53\x49\x51\x54\x38\x54"
        "\x4f\x4a\x4f\x10\x01\x12\x0d\x0a\x03\x45\x55\x52\x10\xc4\x05\x18\x8f\x9a\x9b\x22";
    MessageDirection msg_direction = MessageDirection::kRequest;

    MessageGroupTypeClassifier msg_matcher(&desc_db_, {}, kDefaultParseAsOpts);
    msg_matcher.AddMessage(message, msg_direction);

    std::set<::google::protobuf::MethodDescriptorProto> matching_rpcs;
    matching_rpcs = msg_matcher.MatchingRPCs();

    std::set<std::string> matching_message_types;
    matching_message_types = msg_matcher.MatchingMessageTypes(msg_direction);

    ASSERT_EQ(matching_rpcs.size(), 1);
    EXPECT_EQ(matching_rpcs.begin()->name(), "SendOrderConfirmation");
    EXPECT_EQ(matching_rpcs.begin()->input_type(), ".hipstershop.SendOrderConfirmationRequest");
    EXPECT_EQ(matching_rpcs.begin()->output_type(), ".hipstershop.Empty");

    EXPECT_THAT(matching_rpcs, ElementsAre(HasName("SendOrderConfirmation")));
    EXPECT_THAT(matching_rpcs,
                ElementsAre(HasInputType(".hipstershop.SendOrderConfirmationRequest")));
    EXPECT_THAT(matching_rpcs, ElementsAre(HasOutputType(".hipstershop.Empty")));
  }

  {
    // A hipstershop.CartService/GetCart response.
    std::string message = "\x12\x0e\x0a\x0a\x4f\x4c\x4a\x43\x45\x53\x50\x43\x37\x5a\x10\x01";
    MessageDirection msg_direction = MessageDirection::kResponse;

    MessageGroupTypeClassifier msg_matcher(&desc_db_, {}, kDefaultParseAsOpts);
    msg_matcher.AddMessage(message, msg_direction);

    std::set<::google::protobuf::MethodDescriptorProto> matching_rpcs;
    matching_rpcs = msg_matcher.MatchingRPCs();

    std::set<std::string> matching_message_types;
    matching_message_types = msg_matcher.MatchingMessageTypes(msg_direction);

    auto it = matching_rpcs.begin();
    std::cout << it->input_type() << it->output_type() << std::endl;
    it++;
    std::cout << it->input_type() << it->output_type() << std::endl;

    ASSERT_EQ(matching_rpcs.size(), 2);
    EXPECT_THAT(matching_rpcs, ElementsAre(HasName("GetCart"), HasName("GetProduct")));
    EXPECT_THAT(matching_rpcs, ElementsAre(HasInputType(".hipstershop.GetCartRequest"),
                                           HasInputType(".hipstershop.GetProductRequest")));
    EXPECT_THAT(matching_rpcs, ElementsAre(HasOutputType(".hipstershop.Cart"),
                                           HasOutputType(".hipstershop.Product")));
  }
}

TEST_F(MessageMatcherTest, ReqRespPair) {
  // A hipstershop.CartService/GetCart request/response pair.
  std::string req =
      "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x65\x36\x36\x2d\x34\x65\x66\x64\x2d\x61\x39"
      "\x36\x36\x2d\x35\x62\x37\x62\x32\x36\x61\x36\x35\x65\x65\x61";
  std::string resp = "\x12\x0e\x0a\x0a\x4f\x4c\x4a\x43\x45\x53\x50\x43\x37\x5a\x10\x01";

  MessageGroupTypeClassifier msg_matcher(&desc_db_, {}, kDefaultParseAsOpts);
  std::set<::google::protobuf::MethodDescriptorProto> matching_rpcs;

  matching_rpcs = msg_matcher.MatchingRPCs();
  ASSERT_EQ(matching_rpcs.size(), 15);

  msg_matcher.AddMessage(req, MessageDirection::kRequest);
  matching_rpcs = msg_matcher.MatchingRPCs();
  ASSERT_EQ(matching_rpcs.size(), 9);

  msg_matcher.AddMessage(resp, MessageDirection::kResponse);
  matching_rpcs = msg_matcher.MatchingRPCs();
  ASSERT_EQ(matching_rpcs.size(), 2);
  EXPECT_THAT(matching_rpcs, ElementsAre(HasName("GetCart"), HasName("GetProduct")));
  EXPECT_THAT(matching_rpcs, ElementsAre(HasInputType(".hipstershop.GetCartRequest"),
                                         HasInputType(".hipstershop.GetProductRequest")));
  EXPECT_THAT(matching_rpcs, ElementsAre(HasOutputType(".hipstershop.Cart"),
                                         HasOutputType(".hipstershop.Product")));
}

TEST_F(MessageMatcherTest, NumCandidatesHistory) {
  // A hipstershop.CartService/GetCart request/response pair.
  std::string req =
      "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x65\x36\x36\x2d\x34\x65\x66\x64\x2d\x61\x39"
      "\x36\x36\x2d\x35\x62\x37\x62\x32\x36\x61\x36\x35\x65\x65\x61";
  std::string resp = "\x12\x0e\x0a\x0a\x4f\x4c\x4a\x43\x45\x53\x50\x43\x37\x5a\x10\x01";

  MessageGroupTypeClassifier msg_matcher(&desc_db_, {}, kDefaultParseAsOpts);
  std::set<::google::protobuf::MethodDescriptorProto> matching_rpcs;

  matching_rpcs = msg_matcher.MatchingRPCs();
  msg_matcher.AddMessage(req, MessageDirection::kRequest);
  msg_matcher.AddMessage(resp, MessageDirection::kResponse);
  matching_rpcs = msg_matcher.MatchingRPCs();

  const std::vector<size_t>& num_candidates_history = msg_matcher.num_candidates_history();
  EXPECT_THAT(num_candidates_history, ElementsAre(15, 9, 2));
}

TEST_F(MessageMatcherTest, NumMessages) {
  // A hipstershop.CartService/GetCart request/response pair.
  std::string req =
      "\x0a\x24\x35\x38\x31\x61\x35\x35\x34\x66\x2d\x33\x65\x36\x36\x2d\x34\x65\x66\x64\x2d\x61\x39"
      "\x36\x36\x2d\x35\x62\x37\x62\x32\x36\x61\x36\x35\x65\x65\x61";
  std::string resp = "\x12\x0e\x0a\x0a\x4f\x4c\x4a\x43\x45\x53\x50\x43\x37\x5a\x10\x01";

  MessageGroupTypeClassifier msg_matcher(&desc_db_, {}, kDefaultParseAsOpts);
  msg_matcher.AddMessage(req, MessageDirection::kRequest);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kRequest), 1);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kResponse), 0);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kUnknown), 1);

  msg_matcher.AddMessage(resp, MessageDirection::kResponse);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kRequest), 1);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kResponse), 1);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kUnknown), 2);

  msg_matcher.AddMessage(req, MessageDirection::kRequest);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kRequest), 2);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kResponse), 1);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kUnknown), 3);

  msg_matcher.AddMessage(req, MessageDirection::kRequest);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kRequest), 3);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kResponse), 1);
  EXPECT_EQ(msg_matcher.NumMessages(MessageDirection::kUnknown), 4);
}

TEST(GRPCMessageFlowTest, ReceivesIndexedHeaderFields) {
  GRPCMessageFlow flow;
  EXPECT_THAT(flow.AddMessage({IndexedHeaderField{3}, IndexedHeaderField{6}, IndexedHeaderField{62},
                               IndexedHeaderField{63}}),
              IsOKAndHolds(0ul));
  // The indexed fields in the static table, or the dynamic table, can be reordered. But their
  // relative order cannot.
  EXPECT_THAT(flow.AddMessage({IndexedHeaderField{3}, IndexedHeaderField{62}, IndexedHeaderField{6},
                               IndexedHeaderField{63}}),
              IsOKAndHolds(0ul));
  EXPECT_THAT(
      flow.AddMessage(
          {IndexedHeaderField{3}, IndexedHeaderField{6}, IndexedHeaderField{62},
           IndexedHeaderField{63},
           LiteralHeaderField{/*update_dynamic_table*/ true, /*is_name_huff_encoded*/ true,
                              ConvertToStringView<uint8_t>("abc"), /*is_value_huff_encoded*/ true,
                              ConvertToStringView<uint8_t>("abc")}}),
      IsOKAndHolds(0ul));
  EXPECT_THAT(flow.AddMessage({IndexedHeaderField{3}, IndexedHeaderField{6}, IndexedHeaderField{63},
                               IndexedHeaderField{64}}),
              IsOKAndHolds(0ul));
  EXPECT_THAT(flow.AddMessage({IndexedHeaderField{3}, IndexedHeaderField{6}, IndexedHeaderField{63},
                               LiteralHeaderField{true, true, ConvertToStringView<uint8_t>("abc"),
                                                  true, ConvertToStringView<uint8_t>("abc")}}),
              IsOKAndHolds(1ul));
  EXPECT_THAT(flow.AddMessage({IndexedHeaderField{3}, IndexedHeaderField{6}, IndexedHeaderField{64},
                               IndexedHeaderField{62}}),
              IsOKAndHolds(1ul));

  const auto& message_flows = flow.message_flows_;
  EXPECT_THAT(message_flows, ElementsAre(Pair(Pair(64, 62), 1), Pair(Pair(64, 65), 0)));
}

TEST(GRPCMessageFlowTest, PlainTextHeaderFields) {
  GRPCMessageFlow flow;
  StatusOr<size_t> res = flow.AddMessage(
      {IndexedHeaderField{3}, IndexedHeaderField{6}, IndexedHeaderField{62},
       LiteralHeaderField{/*update_dynamic_table*/ true, /*is_name_huff_encoded*/ false,
                          ConvertToStringView<uint8_t>(":method"), /*is_value_huff_encoded*/ false,
                          ConvertToStringView<uint8_t>("POST")}});
  EXPECT_EQ(statuspb::INVALID_ARGUMENT, res.code());
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
