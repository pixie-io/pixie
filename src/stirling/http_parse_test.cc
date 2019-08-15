#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <utility>

#include "src/stirling/http_parse.h"

namespace pl {
namespace stirling {
namespace http {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

TEST(PreProcessRecordTest, GzipCompressedContentIsDecompressed) {
  HTTPMessage message;
  message.http_headers[kContentEncoding] = "gzip";
  const uint8_t compressed_bytes[] = {0x1f, 0x8b, 0x08, 0x00, 0x37, 0xf0, 0xbf, 0x5c, 0x00,
                                      0x03, 0x0b, 0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44,
                                      0x85, 0x92, 0xd4, 0xe2, 0x12, 0x2e, 0x00, 0x8c, 0x2d,
                                      0xc0, 0xfa, 0x0f, 0x00, 0x00, 0x00};
  message.http_msg_body.assign(reinterpret_cast<const char*>(compressed_bytes),
                               sizeof(compressed_bytes));
  PreProcessMessage(&message);
  EXPECT_EQ("This is a test\n", message.http_msg_body);
}

TEST(PreProcessRecordTest, ContentHeaderIsNotAdded) {
  HTTPMessage message;
  message.http_msg_body = "test";
  PreProcessMessage(&message);
  EXPECT_EQ("test", message.http_msg_body);
  EXPECT_THAT(message.http_headers, Not(Contains(Key(kContentEncoding))));
}

TEST(ParseHTTPHeaderFiltersAndMatchTest, FiltersAreAsExpectedAndMatchesWork) {
  const std::string filters_string =
      "Content-Type:json,,,,Content-Type:plain,Transfer-Encoding:chunked,,,Transfer-Encoding:"
      "chunked,-Content-Encoding:gzip,-Content-Encoding:binary";
  HTTPHeaderFilter filter = ParseHTTPHeaderFilters(filters_string);
  EXPECT_THAT(filter.inclusions,
              ElementsAre(Pair("Content-Type", "json"), Pair("Content-Type", "plain"),
                          Pair("Transfer-Encoding", "chunked"),
                          // Note that multimap does not remove duplicates.
                          Pair("Transfer-Encoding", "chunked")));
  EXPECT_THAT(filter.exclusions,
              ElementsAre(Pair("Content-Encoding", "gzip"), Pair("Content-Encoding", "binary")));
  {
    std::map<std::string, std::string> http_headers = {
        {"Content-Type", "application/json; charset=utf-8"},
    };
    EXPECT_TRUE(MatchesHTTPTHeaders(http_headers, filter));
    http_headers.insert({"Content-Encoding", "gzip"});
    EXPECT_FALSE(MatchesHTTPTHeaders(http_headers, filter)) << "gzip should be filtered out";
  }
  {
    std::map<std::string, std::string> http_headers = {
        {"Transfer-Encoding", "chunked"},
    };
    EXPECT_TRUE(MatchesHTTPTHeaders(http_headers, filter));
    http_headers.insert({"Content-Encoding", "binary"});
    EXPECT_FALSE(MatchesHTTPTHeaders(http_headers, filter)) << "binary should be filtered out";
  }
  {
    std::map<std::string, std::string> http_headers;
    EXPECT_FALSE(MatchesHTTPTHeaders(http_headers, filter));

    const HTTPHeaderFilter empty_filter;
    EXPECT_TRUE(MatchesHTTPTHeaders(http_headers, empty_filter))
        << "Empty filter matches any HTTP headers";
    http_headers.insert({"Content-Type", "non-matching-type"});
    EXPECT_TRUE(MatchesHTTPTHeaders(http_headers, empty_filter))
        << "Empty filter matches any HTTP headers";
  }
  {
    const std::map<std::string, std::string> http_headers = {
        {"Content-Type", "non-matching-type"},
    };
    EXPECT_FALSE(MatchesHTTPTHeaders(http_headers, filter));
  }
}

class HTTPParserTest : public ::testing::Test {
 protected:
  EventParser<HTTPMessage> parser_;
};

bool operator==(const HTTPMessage& lhs, const HTTPMessage& rhs) {
#define CMP(field)                                                 \
  if (lhs.field != rhs.field) {                                    \
    LOG(INFO) << #field ": " << lhs.field << " vs. " << rhs.field; \
    return false;                                                  \
  }
  CMP(http_minor_version);
  CMP(http_resp_status);
  CMP(http_resp_message);
  if (lhs.http_headers != rhs.http_headers) {
    LOG(INFO) << absl::StrJoin(std::begin(lhs.http_headers), std::end(lhs.http_headers), " ",
                               absl::PairFormatter(":"))
              << " vs. "
              << absl::StrJoin(std::begin(rhs.http_headers), std::end(rhs.http_headers), " ",
                               absl::PairFormatter(":"));
    return false;
  }
  if (lhs.type != rhs.type) {
    LOG(INFO) << static_cast<int>(lhs.type) << " vs. " << static_cast<int>(rhs.type);
  }
  return true;
}

socket_data_event_t Event(uint64_t seq_num, std::string_view msg) {
  socket_data_event_t event{};
  event.attr.seq_num = seq_num;
  event.attr.msg_size = msg.size();
  msg.copy(event.msg, msg.size());
  return event;
}

TEST_F(HTTPParserTest, ParseCompleteHTTPResponseWithContentLengthHeader) {
  std::string_view msg1 = R"(HTTP/1.1 200 OK
Content-Type: foo
Content-Length: 9

pixielabs)";

  std::string_view msg2 = R"(HTTP/1.1 200 OK
Content-Type: bar
Content-Length: 10

pixielabs!)";

  HTTPMessage expected_message1;
  expected_message1.type = HTTPEventType::kHTTPResponse;
  expected_message1.http_minor_version = 1;
  expected_message1.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "9"}};
  expected_message1.http_resp_status = 200;
  expected_message1.http_resp_message = "OK";
  expected_message1.http_msg_body = "pixielabs";

  HTTPMessage expected_message2;
  expected_message2.type = HTTPEventType::kHTTPResponse;
  expected_message2.http_minor_version = 1;
  expected_message2.http_headers = {{"Content-Type", "bar"}, {"Content-Length", "10"}};
  expected_message2.http_resp_status = 200;
  expected_message2.http_resp_message = "OK";
  expected_message2.http_msg_body = "pixielabs!";

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1, expected_message2));
}

TEST_F(HTTPParserTest, ParseIncompleteHTTPResponseWithContentLengthHeader) {
  const std::string_view msg1 = R"(HTTP/1.1 200 OK
Content-Type: foo
Content-Length: 21

pixielabs)";
  const std::string_view msg2 = " is awesome";
  const std::string_view msg3 = "!";

  HTTPMessage expected_message1;
  expected_message1.type = HTTPEventType::kHTTPResponse;
  expected_message1.http_minor_version = 1;
  expected_message1.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "21"}};
  expected_message1.http_resp_status = 200;
  expected_message1.http_resp_message = "OK";
  expected_message1.http_msg_body = "pixielabs is awesome!";

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);
  parser_.Append(msg3, 2);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message1));
}

TEST_F(HTTPParserTest, InvalidInput) {
  const std::string_view msg = " is awesome";

  parser_.Append(msg, 0);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kInvalid, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

TEST_F(HTTPParserTest, NoAppend) {
  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

// Leave http_msg_body set by caller.
HTTPMessage ExpectMessage() {
  HTTPMessage result;
  result.type = HTTPEventType::kHTTPResponse;
  result.http_minor_version = 1;
  result.http_headers = {{"Transfer-Encoding", "chunked"}};
  result.http_resp_status = 200;
  result.http_resp_message = "OK";
  return result;
}

TEST_F(HTTPParserTest, ParseCompleteChunkEncodedMessage) {
  std::string msg = R"(HTTP/1.1 200 OK
Transfer-Encoding: chunked

9
pixielabs
C
 is awesome!
0

)";
  HTTPMessage expected_message = ExpectMessage();
  expected_message.http_msg_body = "pixielabs is awesome!";

  parser_.Append(msg, 0);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message));
}

TEST_F(HTTPParserTest, ParseMultipleChunks) {
  std::string msg1 = R"(HTTP/1.1 200 OK
Transfer-Encoding: chunked

9
pixielabs
)";
  std::string msg2 = "C\r\n is awesome!\r\n";
  std::string msg3 = "0\r\n\r\n";

  HTTPMessage expected_message = ExpectMessage();
  expected_message.http_msg_body = "pixielabs is awesome!";

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);
  parser_.Append(msg3, 2);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message));
}

TEST_F(HTTPParserTest, ParseIncompleteChunks) {
  std::string msg1 = R"(HTTP/1.1 200 OK
Transfer-Encoding: chunked

9
pixie)";
  std::string msg2 = "labs\r\n";
  std::string msg3 = "0\r\n\r\n";

  HTTPMessage expected_message = ExpectMessage();
  expected_message.http_msg_body = "pixielabs";

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);
  parser_.Append(msg3, 2);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message));
}

TEST_F(HTTPParserTest, DISABLED_ParseMessagesWithoutLengthOrChunking) {
  std::string msg1 = R"(HTTP/1.1 200 OK

pixielabs )";
  std::string msg2 = "is ";
  std::string msg3 = "awesome!";

  HTTPMessage expected_message = ExpectMessage();
  expected_message.http_headers.clear();
  expected_message.http_msg_body = "pixielabs is awesome!";

  parser_.Append(msg1, 0);
  parser_.Append(msg2, 1);
  parser_.Append(msg3, 2);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(expected_message));
}

std::string HTTPRespWithSizedBody(std::string_view body) {
  return absl::Substitute(
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: $0\r\n"
      "\r\n"
      "$1",
      body.size(), body);
}

std::string HTTPChunk(std::string_view chunk_body) {
  const char size = chunk_body.size() < 10 ? '0' + chunk_body.size() : 'A' + chunk_body.size() - 10;
  std::string s(1, size);
  return absl::StrCat(s, "\r\n", chunk_body, "\r\n");
}

std::string HTTPRespWithChunkedBody(std::vector<std::string_view> chunk_bodys) {
  std::string result =
      "HTTP/1.1 200 OK\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n";
  for (auto c : chunk_bodys) {
    absl::StrAppend(&result, HTTPChunk(c));
  }
  // Lastly, append a 0-length chunk.
  absl::StrAppend(&result, HTTPChunk(""));
  return result;
}

TEST_F(HTTPParserTest, MessagePartialHeaders) {
  std::string msg1 = R"(HTTP/1.1 200 OK
Content-Type: text/plain)";

  parser_.Append(msg1, 0);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kNeedsMoreData, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre());
}

MATCHER_P(HasBody, body, "") {
  return arg.http_msg_body == body;
}

TEST_F(HTTPParserTest, PartialMessageInTheMiddleOfStream) {
  std::string msg0 = HTTPRespWithSizedBody("foobar") + "HTTP/1.1 200 OK\r\n";
  std::string msg1 = "Transfer-Encoding: chunked\r\n\r\n";
  std::string msg2 = HTTPChunk("pixielabs ");
  std::string msg3 = HTTPChunk("rocks!");
  std::string msg4 = HTTPChunk("");

  parser_.Append(msg0, 0);
  parser_.Append(msg1, 1);
  parser_.Append(msg2, 2);
  parser_.Append(msg3, 3);
  parser_.Append(msg4, 4);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(HasBody("foobar"), HasBody("pixielabs rocks!")));
}

TEST(ParseTest, CompleteMessages) {
  std::string msg_a = HTTPRespWithSizedBody("a");
  std::string msg_b = HTTPRespWithChunkedBody({"b"});
  std::string msg_c = HTTPRespWithSizedBody("c");
  std::string buf = absl::StrCat(msg_a, msg_b, msg_c);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = Parse(MessageType::kResponse, buf, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_EQ(msg_a.size() + msg_b.size() + msg_c.size(), result.end_position);
  EXPECT_THAT(parsed_messages, ElementsAre(HasBody("a"), HasBody("b"), HasBody("c")));
  EXPECT_THAT(result.start_positions, ElementsAre(0, msg_a.size(), msg_a.size() + msg_b.size()));
}

TEST(ParseTest, PartialMessages) {
  std::string msg =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: $0\r\n";

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = Parse(MessageType::kResponse, msg, &parsed_messages);

  EXPECT_EQ(ParseState::kNeedsMoreData, result.state);
  EXPECT_EQ(0, result.end_position);
  EXPECT_THAT(parsed_messages, IsEmpty());
}

//=============================================================================
// HTTP Request Parsing Tests
//=============================================================================

// Parameter used for stress/fuzz test.
struct TestParam {
  uint32_t seed;
  uint32_t iters;
};

class HTTPParserStressTest : public ::testing::TestWithParam<TestParam> {
 protected:
  EventParser<HTTPMessage> parser_;

  const std::string kHTTPGetReq0 = R"(GET /index.html HTTP/1.1
Host: www.pixielabs.ai
Accept: image/gif, image/jpeg, */*
User-Agent: Mozilla/5.0 (X11; Linux x86_64)

)";

  HTTPMessage HTTPGetReq0ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.type = HTTPEventType::kHTTPRequest;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Host", "www.pixielabs.ai"},
                                     {"Accept", "image/gif, image/jpeg, */*"},
                                     {"User-Agent", "Mozilla/5.0 (X11; Linux x86_64)"}};
    expected_message.http_req_method = "GET";
    expected_message.http_req_path = "/index.html";
    expected_message.http_msg_body = "-";
    return expected_message;
  }

  const std::string kHTTPPostReq0 = R"(POST /test HTTP/1.1
Host: pixielabs.ai
Content-Type: application/x-www-form-urlencoded
Content-Length: 27

field1=value1&field2=value2)";

  HTTPMessage HTTPPostReq0ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.type = HTTPEventType::kHTTPRequest;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Host", "pixielabs.ai"},
                                     {"Content-Type", "application/x-www-form-urlencoded"},
                                     {"Content-Length", "27"}};
    expected_message.http_req_method = "POST";
    expected_message.http_req_path = "/test";
    expected_message.http_msg_body = "field1=value1&field2=value2";
    return expected_message;
  }

  std::string kHTTPResp0 = R"(HTTP/1.1 200 OK
Content-Type: foo
Content-Length: 9

pixielabs)";

  HTTPMessage HTTPResp0ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.type = HTTPEventType::kHTTPResponse;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "9"}};
    expected_message.http_resp_status = 200;
    expected_message.http_resp_message = "OK";
    expected_message.http_msg_body = "pixielabs";
    return expected_message;
  }

  std::string kHTTPResp1 = R"(HTTP/1.1 200 OK
Content-Type: bar
Content-Length: 21

pixielabs is awesome!)";

  HTTPMessage HTTPResp1ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.type = HTTPEventType::kHTTPResponse;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Content-Type", "bar"}, {"Content-Length", "21"}};
    expected_message.http_resp_status = 200;
    expected_message.http_resp_message = "OK";
    expected_message.http_msg_body = "pixielabs is awesome!";
    return expected_message;
  }

  std::string kHTTPResp2 = R"(HTTP/1.1 200 OK
Transfer-Encoding: chunked

9
pixielabs
C
 is awesome!
0

)";

  HTTPMessage HTTPResp2ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.type = HTTPEventType::kHTTPResponse;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Transfer-Encoding", "chunked"}};
    expected_message.http_resp_status = 200;
    expected_message.http_resp_message = "OK";
    expected_message.http_msg_body = "pixielabs is awesome!";
    return expected_message;
  }

  // Utility function that takes a string view of a buffer, and a set of N split points,
  // and returns a set of N+1 split string_views of the buffer.
  std::vector<std::string_view> MessageSplit(std::string_view msg,
                                             std::vector<size_t> split_points) {
    std::vector<std::string_view> splits;

    split_points.push_back(msg.length());
    std::sort(split_points.begin(), split_points.end());

    // Check for bad split_points.
    CHECK_EQ(msg.length(), split_points.back());

    uint32_t curr_pos = 0;
    for (auto split_point : split_points) {
      auto split = std::string_view(msg).substr(curr_pos, split_point - curr_pos);
      splits.push_back(split);
      curr_pos = split_point;
    }

    return splits;
  }
};

TEST_F(HTTPParserStressTest, ParseHTTPRequestSingle) {
  parser_.Append(kHTTPGetReq0, 0);

  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages, ElementsAre(HTTPGetReq0ExpectedMessage()));
}

TEST_F(HTTPParserStressTest, ParseHTTPRequestMultiple) {
  parser_.Append(kHTTPGetReq0, 0);
  parser_.Append(kHTTPPostReq0, 1);
  std::deque<HTTPMessage> parsed_messages;
  ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

  EXPECT_EQ(ParseState::kSuccess, result.state);
  EXPECT_THAT(parsed_messages,
              ElementsAre(HTTPGetReq0ExpectedMessage(), HTTPPostReq0ExpectedMessage()));
}

TEST_P(HTTPParserStressTest, ParseHTTPRequestsRepeatedly) {
  std::string msg = kHTTPGetReq0 + kHTTPPostReq0;

  std::default_random_engine rng;
  rng.seed(GetParam().seed);
  std::uniform_int_distribution<uint32_t> splitpoint_dist(0, msg.length());

  for (uint32_t i = 0; i < GetParam().iters; ++i) {
    // Choose two random split points for this iteration.
    std::vector<size_t> split_points;
    split_points.push_back(splitpoint_dist(rng));
    split_points.push_back(splitpoint_dist(rng));

    std::vector<std::string_view> msg_splits = MessageSplit(msg, split_points);

    ASSERT_EQ(msg, absl::StrCat(msg_splits[0], msg_splits[1], msg_splits[2]));

    parser_.Append(msg_splits[0], i * 3 + 0);
    parser_.Append(msg_splits[1], i * 3 + 1);
    parser_.Append(msg_splits[2], i * 3 + 2);

    std::deque<HTTPMessage> parsed_messages;
    ParseResult result = parser_.ParseMessages(MessageType::kRequest, &parsed_messages);

    ASSERT_EQ(ParseState::kSuccess, result.state);
    ASSERT_THAT(parsed_messages,
                ElementsAre(HTTPGetReq0ExpectedMessage(), HTTPPostReq0ExpectedMessage()));
  }
}

TEST_P(HTTPParserStressTest, ParseHTTPResponsesRepeatedly) {
  std::string msg = kHTTPResp0 + kHTTPResp1 + kHTTPResp2;

  std::default_random_engine rng;
  rng.seed(GetParam().seed);
  std::uniform_int_distribution<size_t> splitpoint_dist(0, msg.length());

  for (uint32_t i = 0; i < GetParam().iters; ++i) {
    // Choose two random split points for this iteration.
    std::vector<size_t> split_points;
    split_points.push_back(splitpoint_dist(rng));
    split_points.push_back(splitpoint_dist(rng));

    std::vector<std::string_view> msg_splits = MessageSplit(msg, split_points);

    ASSERT_EQ(msg, absl::StrCat(msg_splits[0], msg_splits[1], msg_splits[2]));

    parser_.Append(msg_splits[0], i * 3 + 0);
    parser_.Append(msg_splits[1], i * 3 + 1);
    parser_.Append(msg_splits[2], i * 3 + 2);

    std::deque<HTTPMessage> parsed_messages;
    ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

    ASSERT_EQ(ParseState::kSuccess, result.state);
    ASSERT_THAT(parsed_messages, ElementsAre(HTTPResp0ExpectedMessage(), HTTPResp1ExpectedMessage(),
                                             HTTPResp2ExpectedMessage()));
  }
}

// Tests the case where the ParseMessages results in some leftover unprocessed data
// that needs to be processed after more data is added to the buffer.
// ParseHTTPResponseWithLeftoverRepeatedly expands on this by repeating this process many times.
// Keeping this test as a basic filter (easier for debug).
TEST_F(HTTPParserStressTest, ParseHTTPResponsesWithLeftover) {
  std::string msg = kHTTPResp0 + kHTTPResp1 + kHTTPResp2;

  std::vector<size_t> split_points;
  split_points.push_back(kHTTPResp0.length() - 5);
  split_points.push_back(msg.size() - 10);
  std::vector<std::string_view> msg_splits = MessageSplit(msg, split_points);

  ASSERT_EQ(msg, absl::StrCat(msg_splits[0], msg_splits[1], msg_splits[2]));

  std::deque<HTTPMessage> parsed_messages;

  parser_.Append(msg_splits[0], 0);
  parser_.Append(msg_splits[1], 1);
  // Don't append last split, yet.

  ParseResult result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  ASSERT_EQ(ParseState::kNeedsMoreData, result.state);
  ASSERT_THAT(parsed_messages, ElementsAre(HTTPResp0ExpectedMessage(), HTTPResp1ExpectedMessage()));

  BufferPosition position = result.end_position;
  size_t offset = position.offset;

  // Now append the unprocessed remainder, including msg_splits[2].
  for (size_t i = position.seq_num; i < msg_splits.size(); ++i) {
    std::string_view t = msg_splits[i].substr(offset);
    parser_.Append(t, 1);
    offset = 0;
  }

  result = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

  ASSERT_EQ(ParseState::kSuccess, result.state);
  ASSERT_THAT(parsed_messages, ElementsAre(HTTPResp0ExpectedMessage(), HTTPResp1ExpectedMessage(),
                                           HTTPResp2ExpectedMessage()));
}

// Like ParseHTTPResponsesWithLeftover, but repeats test many times,
// each time with different random split points to stress the functionality.
TEST_P(HTTPParserStressTest, ParseHTTPResponsesWithLeftoverRepeatedly) {
  std::string msg = kHTTPResp0 + kHTTPResp1 + kHTTPResp2 + kHTTPResp1;

  std::default_random_engine rng;
  rng.seed(GetParam().seed);
  std::uniform_int_distribution<size_t> splitpoint_dist(0, msg.length());

  for (uint32_t j = 0; j < GetParam().iters; ++j) {
    // Choose two random split points for this iteration.
    std::vector<size_t> split_points;
    split_points.push_back(splitpoint_dist(rng));
    split_points.push_back(splitpoint_dist(rng));
    std::vector<std::string_view> msg_splits = MessageSplit(msg, split_points);

    ASSERT_EQ(msg, absl::StrCat(msg_splits[0], msg_splits[1], msg_splits[2]));

    std::deque<HTTPMessage> parsed_messages;

    // Append and parse some--but not all--splits.
    parser_.Append(msg_splits[0], 0);
    parser_.Append(msg_splits[1], 0);
    ParseResult result1 = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

    // Now append the unprocessed remainder, including msg_splits[2].
    BufferPosition position = result1.end_position;
    size_t offset = position.offset;
    for (size_t i = position.seq_num; i < msg_splits.size(); ++i) {
      std::string_view t = msg_splits[i].substr(offset);
      parser_.Append(t, 0);
      offset = 0;
    }
    ParseResult result2 = parser_.ParseMessages(MessageType::kResponse, &parsed_messages);

    ASSERT_EQ(ParseState::kSuccess, result2.state);
    ASSERT_THAT(parsed_messages,
                ElementsAre(HTTPResp0ExpectedMessage(), HTTPResp1ExpectedMessage(),
                            HTTPResp2ExpectedMessage(), HTTPResp1ExpectedMessage()));
  }
}

INSTANTIATE_TEST_CASE_P(Stressor, HTTPParserStressTest,
                        ::testing::Values(TestParam{37337, 50}, TestParam{98237, 50}));
// TODO(oazizi/yzhao): TestParam{37337, 100} fails, so there is a bug somewhere. Fix it.

}  // namespace http
}  // namespace stirling
}  // namespace pl
