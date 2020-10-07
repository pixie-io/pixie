#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/dns/dns_parse.h"

namespace pl {
namespace stirling {
namespace dns {

// A query captured via WireShark:
//   Domain Name System (query)
//   Transaction ID: 0xc6fa
//   Flags: 0x0100 Standard query
//   Questions: 1
//   Answer RRs: 0
//   Authority RRs: 0
//   Additional RRs: 1
//   Queries
//           intellij-experiments.appspot.com: type A, class IN
//   Additional records
constexpr uint8_t kQueryFrame[] = {
    0xc6, 0xfa, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x14, 0x69, 0x6e, 0x74,
    0x65, 0x6c, 0x6c, 0x69, 0x6a, 0x2d, 0x65, 0x78, 0x70, 0x65, 0x72, 0x69, 0x6d, 0x65, 0x6e, 0x74,
    0x73, 0x07, 0x61, 0x70, 0x70, 0x73, 0x70, 0x6f, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// The corresponding response to the query above.
//   Domain Name System (response)
//   Transaction ID: 0xc6fa
//   Flags: 0x8180 Standard query response, No error
//   Questions: 1
//   Answer RRs: 1
//   Authority RRs: 0
//   Additional RRs: 1
//   Queries
//           intellij-experiments.appspot.com: type A, class IN
//   Answers
//           intellij-experiments.appspot.com: type A, class IN, addr 216.58.194.180
//   Additional records
constexpr uint8_t kRespFrame[] = {
    0xc6, 0xfa, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x14, 0x69, 0x6e, 0x74,
    0x65, 0x6c, 0x6c, 0x69, 0x6a, 0x2d, 0x65, 0x78, 0x70, 0x65, 0x72, 0x69, 0x6d, 0x65, 0x6e, 0x74,
    0x73, 0x07, 0x61, 0x70, 0x70, 0x73, 0x70, 0x6f, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
    0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x24, 0x00, 0x04, 0xd8, 0x3a,
    0xc2, 0xb4, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

class CQLParserTest : public ::testing::Test {
 protected:
  EventParser parser_;
};

TEST_F(CQLParserTest, BasicReq) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kQueryFrame));

  std::deque<Frame> frames;
  ParseResult<size_t> parse_result =
      parser_.ParseFramesLoop(MessageType::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0].header.txid, 0xc6fa);
  EXPECT_EQ(frames[0].header.flags, 0x0100);
  EXPECT_EQ(frames[0].header.num_queries, 1);
  EXPECT_EQ(frames[0].header.num_answers, 0);
  EXPECT_EQ(frames[0].header.num_auth, 0);
  EXPECT_EQ(frames[0].header.num_addl, 1);
  EXPECT_EQ(frames[0].records.size(), 0);
}

TEST_F(CQLParserTest, BasicResp) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame));

  std::deque<Frame> frames;
  ParseResult<size_t> parse_result =
      parser_.ParseFramesLoop(MessageType::kResponse, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);
  ASSERT_EQ(frames.size(), 1);
  EXPECT_EQ(frames[0].header.txid, 0xc6fa);
  EXPECT_EQ(frames[0].header.flags, 0x8180);
  EXPECT_EQ(frames[0].header.num_queries, 1);
  EXPECT_EQ(frames[0].header.num_answers, 1);
  EXPECT_EQ(frames[0].header.num_auth, 0);
  EXPECT_EQ(frames[0].header.num_addl, 1);
  EXPECT_EQ(frames[0].records.size(), 1);
  EXPECT_EQ(frames[0].records[0].name, "intellij-experiments.appspot.com");
  EXPECT_EQ(frames[0].records[0].addr.family, SockAddrFamily::kIPv4);
  EXPECT_EQ(frames[0].records[0].addr.AddrStr(), "216.58.194.180");
}

TEST_F(CQLParserTest, IncompleteHeader) {
  constexpr uint8_t kIncompleteHeader[] = {0xc6, 0xfa, 0x01, 0x00, 0x00, 0x01,
                                           0x00, 0x00, 0x00, 0x00, 0x00};
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kIncompleteHeader));

  std::deque<Frame> frames;
  ParseResult<size_t> parse_result =
      parser_.ParseFramesLoop(MessageType::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
}

// NOTE that some partial records parse correctly, while others don't.
// Should modify the submodule so that all are reported as invalid.
TEST_F(CQLParserTest, PartialRecords) {
  {
    auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame));
    frame_view.remove_suffix(10);

    std::deque<Frame> frames;
    ParseResult<size_t> parse_result =
        parser_.ParseFramesLoop(MessageType::kRequest, frame_view, &frames);

    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
  }

  {
    auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame));
    frame_view.remove_suffix(20);

    std::deque<Frame> frames;
    ParseResult<size_t> parse_result =
        parser_.ParseFramesLoop(MessageType::kRequest, frame_view, &frames);

    ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  }
}

}  // namespace dns
}  // namespace stirling
}  // namespace pl
