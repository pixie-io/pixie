#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <utility>

#include "src/stirling/http_parse.h"

namespace pl {
namespace stirling {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

TEST(PreProcessRecordTest, GzipCompressedContentIsDecompressed) {
  HTTPTraceRecord record;
  record.message.http_headers[http_headers::kContentEncoding] = "gzip";
  const uint8_t compressed_bytes[] = {0x1f, 0x8b, 0x08, 0x00, 0x37, 0xf0, 0xbf, 0x5c, 0x00,
                                      0x03, 0x0b, 0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44,
                                      0x85, 0x92, 0xd4, 0xe2, 0x12, 0x2e, 0x00, 0x8c, 0x2d,
                                      0xc0, 0xfa, 0x0f, 0x00, 0x00, 0x00};
  record.message.http_msg_body.assign(reinterpret_cast<const char*>(compressed_bytes),
                                      sizeof(compressed_bytes));
  PreProcessHTTPRecord(&record);
  EXPECT_EQ("This is a test\n", record.message.http_msg_body);
}

TEST(PreProcessRecordTest, ContentHeaderIsNotAdded) {
  HTTPTraceRecord record;
  record.message.http_msg_body = "test";
  PreProcessHTTPRecord(&record);
  EXPECT_EQ("test", record.message.http_msg_body);
  EXPECT_THAT(record.message.http_headers, Not(Contains(Key(http_headers::kContentEncoding))));
}

TEST(ParseEventAttrTest, DataIsCopied) {
  socket_data_event_t event;
  event.attr.timestamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.fd = 3;

  HTTPTraceRecord record;

  ParseEventAttr(event, &record);

  EXPECT_EQ(100, record.message.timestamp_ns);
  EXPECT_EQ(1, record.conn.tgid);
  EXPECT_EQ(3, record.conn.fd);
}

TEST(ParseRawTest, ContentIsCopied) {
  const std::string data = "test";
  socket_data_event_t event;
  event.attr.timestamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.fd = 3;
  event.attr.msg_size = data.size();
  data.copy(event.msg, data.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseRaw(event, &record));
  EXPECT_EQ(100, record.message.timestamp_ns);
  EXPECT_EQ(1, record.conn.tgid);
  EXPECT_EQ(3, record.conn.fd);
  EXPECT_EQ(SocketTraceEventType::kUnknown, record.message.type);
  EXPECT_EQ("test", record.message.http_msg_body);
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
  HTTPParser parser_;
};

bool operator==(const HTTPMessage& lhs, const HTTPMessage& rhs) {
#define CMP(field)                                                 \
  if (lhs.field != rhs.field) {                                    \
    LOG(INFO) << #field ": " << lhs.field << " vs. " << rhs.field; \
    return false;                                                  \
  }
  CMP(is_complete);
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
  socket_data_event_t event;
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
  expected_message1.is_complete = true;
  expected_message1.type = SocketTraceEventType::kHTTPResponse;
  expected_message1.http_minor_version = 1;
  expected_message1.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "9"}};
  expected_message1.http_resp_status = 200;
  expected_message1.http_resp_message = "OK";
  expected_message1.http_msg_body = "pixielabs";

  HTTPMessage expected_message2;
  expected_message2.is_complete = true;
  expected_message2.type = SocketTraceEventType::kHTTPResponse;
  expected_message2.http_minor_version = 1;
  expected_message2.http_headers = {{"Content-Type", "bar"}, {"Content-Length", "10"}};
  expected_message2.http_resp_status = 200;
  expected_message2.http_resp_message = "OK";
  expected_message2.http_msg_body = "pixielabs!";

  parser_.Append(0, 0, msg1);
  parser_.Append(1, 1, msg2);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message1, expected_message2));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

TEST_F(HTTPParserTest, ParseIncompleteHTTPResponseWithContentLengthHeader) {
  const std::string_view msg1 = R"(HTTP/1.1 200 OK
Content-Type: foo
Content-Length: 21

pixielabs)";
  const std::string_view msg2 = " is awesome";
  const std::string_view msg3 = "!";

  HTTPMessage expected_message1;
  expected_message1.is_complete = true;
  expected_message1.type = SocketTraceEventType::kHTTPResponse;
  expected_message1.http_minor_version = 1;
  expected_message1.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "21"}};
  expected_message1.http_resp_status = 200;
  expected_message1.http_resp_message = "OK";
  expected_message1.http_msg_body = "pixielabs is awesome!";

  parser_.Append(0, 0, msg1);
  parser_.Append(1, 1, msg2);
  parser_.Append(2, 2, msg3);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message1));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

TEST_F(HTTPParserTest, InvalidInput) {
  const std::string_view msg = " is awesome";

  {
    parser_.Append(0, 0, msg);
    parser_.ParseMessages(kMessageTypeResponses);

    EXPECT_EQ(ParseState::kInvalid, parser_.parse_state());
    EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  }
  {
    parser_.Append(2, 0, msg);
    parser_.ParseMessages(kMessageTypeResponses);

    EXPECT_EQ(ParseState::kInvalid, parser_.parse_state());
    EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  }
}

TEST_F(HTTPParserTest, NoAppend) {
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

// Leave http_msg_body set by caller.
HTTPMessage ExpectMessage() {
  HTTPMessage result;
  result.is_complete = true;
  result.type = SocketTraceEventType::kHTTPResponse;
  result.http_minor_version = 1;
  result.http_headers = {{"Transfer-Encoding", "chunked"}};
  result.http_resp_status = 200;
  result.http_resp_message = "OK";
  return result;
}

TEST_F(HTTPParserTest, ParseComplteChunkEncodedMessage) {
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

  parser_.Append(0, 0, msg);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
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

  parser_.Append(0, 0, msg1);
  parser_.Append(1, 1, msg2);
  parser_.Append(2, 2, msg3);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty()) << "Data should be empty after extraction";
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

  parser_.Append(0, 0, msg1);
  parser_.Append(1, 1, msg2);
  parser_.Append(2, 2, msg3);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty()) << "Data should be empty after extraction";
}

TEST_F(HTTPParserTest, ParseMessagesWithoutLengthOrChunking) {
  std::string msg1 = R"(HTTP/1.1 200 OK

pixielabs )";
  std::string msg2 = "is ";
  std::string msg3 = "awesome!";

  HTTPMessage expected_message = ExpectMessage();
  expected_message.http_headers.clear();
  expected_message.http_msg_body = "pixielabs is awesome!";

  parser_.Append(0, 0, msg1);
  parser_.Append(1, 1, msg2);
  parser_.Append(2, 2, msg3);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  parser_.Close();
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty()) << "Data should be empty after extraction";
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

  parser_.Append(0, 0, msg1);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kNeedsMoreData, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

MATCHER_P(HasBody, body, "") {
  LOG(INFO) << "Got: " << arg.http_msg_body;
  return arg.http_msg_body == body;
}

TEST_F(HTTPParserTest, PartialMessageInTheMiddleOfStream) {
  std::string msg0 = HTTPRespWithSizedBody("foobar") + "HTTP/1.1 200 OK\r\n";
  std::string msg1 = "Transfer-Encoding: chunked\r\n\r\n";
  std::string msg2 = HTTPChunk("pixielabs ");
  std::string msg3 = HTTPChunk("rocks!");
  std::string msg4 = HTTPChunk("");

  parser_.Append(0, 0, msg0);
  parser_.Append(1, 1, msg1);
  parser_.Append(2, 2, msg2);
  parser_.Append(3, 3, msg3);
  parser_.Append(4, 4, msg4);
  parser_.ParseMessages(kMessageTypeResponses);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(),
              ElementsAre(HasBody("foobar"), HasBody("pixielabs rocks!")));
}

TEST_F(HTTPParserTest, AppendNonContiguousMessage) {
  EXPECT_TRUE(parser_.Append(1, 0, ""));
  EXPECT_FALSE(parser_.Append(3, 0, ""));
}

MATCHER_P2(HasBytesRange, begin, end, "") {
  LOG(INFO) << "Got: " << arg.bytes_begin << ", " << arg.bytes_end;
  return arg.bytes_begin == static_cast<size_t>(begin) && arg.bytes_end == static_cast<size_t>(end);
}

TEST(ParseTest, CompleteMessages) {
  std::string msg_a = HTTPRespWithSizedBody("a");
  std::string msg_b = HTTPRespWithChunkedBody({"b"});
  std::string msg_c = HTTPRespWithSizedBody("c");
  std::string buf = absl::StrCat(msg_a, msg_b, msg_c);

  std::vector<SeqHTTPMessage> messages;
  EXPECT_EQ(std::make_pair(ParseState::kSuccess, msg_a.size() + msg_b.size() + msg_c.size()),
            Parse(kMessageTypeResponses, buf, &messages));
  EXPECT_THAT(messages, ElementsAre(HasBody("a"), HasBody("b"), HasBody("c")));
  EXPECT_THAT(messages, ElementsAre(HasBytesRange(0, msg_a.size()),
                                    HasBytesRange(msg_a.size(), msg_a.size() + msg_b.size()),
                                    HasBytesRange(msg_a.size() + msg_b.size(),
                                                  msg_a.size() + msg_b.size() + msg_c.size())));
}

TEST(ParseTest, PartialMessages) {
  std::string msg =
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: $0\r\n";

  std::vector<SeqHTTPMessage> messages;
  constexpr size_t kZero = 0;
  EXPECT_EQ(std::make_pair(ParseState::kNeedsMoreData, kZero),
            Parse(kMessageTypeResponses, msg, &messages));
  EXPECT_THAT(messages, IsEmpty());
}

//=============================================================================
// HTTP Request Parsing Tests
//=============================================================================

// Parameter used for stress/fuzz test.
struct TestParam {
  uint32_t seed;
  uint32_t iters;
};

class HTTPRequestParserTest : public ::testing::TestWithParam<TestParam> {
 protected:
  HTTPParser parser_;

  const std::string kHTTPGetReq0 = R"(GET /index.html HTTP/1.1
Host: www.pixielabs.ai
Accept: image/gif, image/jpeg, */*
User-Agent: Mozilla/5.0 (X11; Linux x86_64)

)";

  const std::string kHTTPPostReq0 = R"(POST /test HTTP/1.1
Host: pixielabs.ai
Content-Type: application/x-www-form-urlencoded
Content-Length: 27

field1=value1&field2=value2)";

  HTTPMessage HTTPGetReq0ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.is_complete = true;
    expected_message.type = SocketTraceEventType::kHTTPRequest;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Host", "www.pixielabs.ai"},
                                     {"Accept", "image/gif, image/jpeg, */*"},
                                     {"User-Agent", "Mozilla/5.0 (X11; Linux x86_64)"}};
    expected_message.http_req_method = "GET";
    expected_message.http_req_path = "/index.html";
    expected_message.http_msg_body = "-";
    return expected_message;
  }

  HTTPMessage HTTPPostReq0ExpectedMessage() {
    HTTPMessage expected_message;
    expected_message.is_complete = true;
    expected_message.type = SocketTraceEventType::kHTTPRequest;
    expected_message.http_minor_version = 1;
    expected_message.http_headers = {{"Host", "pixielabs.ai"},
                                     {"Content-Type", "application/x-www-form-urlencoded"},
                                     {"Content-Length", "27"}};
    expected_message.http_req_method = "POST";
    expected_message.http_req_path = "/test";
    expected_message.http_msg_body = "field1=value1&field2=value2";
    return expected_message;
  }
};

TEST_F(HTTPRequestParserTest, ParseHTTPRequestSingle) {
  parser_.Append(0, 0, kHTTPGetReq0);
  parser_.ParseMessages(kMessageTypeRequests);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(HTTPGetReq0ExpectedMessage()));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

TEST_F(HTTPRequestParserTest, ParseHTTPRequestMultiple) {
  parser_.Append(0, 0, kHTTPGetReq0);
  parser_.Append(1, 1, kHTTPPostReq0);
  parser_.ParseMessages(kMessageTypeRequests);

  EXPECT_EQ(ParseState::kSuccess, parser_.parse_state());
  EXPECT_THAT(parser_.ExtractHTTPMessages(),
              ElementsAre(HTTPGetReq0ExpectedMessage(), HTTPPostReq0ExpectedMessage()));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

TEST_P(HTTPRequestParserTest, ParseHTTPRequestsRepeatedly) {
  std::string msg = kHTTPGetReq0 + kHTTPPostReq0;

  std::default_random_engine rng;
  rng.seed(GetParam().seed);
  std::uniform_int_distribution<uint32_t> splitpoint_dist(0, msg.length());

  for (uint32_t i = 0; i < GetParam().iters; ++i) {
    uint32_t r1 = splitpoint_dist(rng);
    uint32_t r2 = splitpoint_dist(rng);

    uint32_t k1 = std::min(r1, r2);
    uint32_t k2 = std::max(r1, r2);

    auto s1 = std::string_view(msg).substr(0, k1 - 0);
    auto s2 = std::string_view(msg).substr(k1, k2 - k1);
    auto s3 = std::string_view(msg).substr(k2, msg.length() - k2);

    ASSERT_EQ(msg.length(), s1.length() + s2.length() + s3.length());
    ASSERT_EQ(msg, std::string(s1) + std::string(s2) + std::string(s3));

    parser_.Append(0, 0, s1);
    parser_.Append(1, 1, s2);
    parser_.Append(2, 2, s3);

    parser_.ParseMessages(kMessageTypeRequests);

    ASSERT_EQ(ParseState::kSuccess, parser_.parse_state());
    ASSERT_THAT(parser_.ExtractHTTPMessages(),
                ElementsAre(HTTPGetReq0ExpectedMessage(), HTTPPostReq0ExpectedMessage()));
    ASSERT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  }
}

INSTANTIATE_TEST_CASE_P(Stressor, HTTPRequestParserTest,
                        ::testing::Values(TestParam{37337, 50}, TestParam{98237, 50}));
// TODO(oazizi/yzhao): TestParam{37337, 100} fails, so there is a bug somewhere. Fix it.

}  // namespace stirling
}  // namespace pl
