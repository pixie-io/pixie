#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/http_parse.h"

namespace pl {
namespace stirling {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

TEST(ParseChunkedMessageBodyTest, DetectChunkedMessage) {
  {
    HTTPTraceRecord record;
    record.http_resp_body = "9\r\nPixieLabs\r\n0\r\n\r\n";
    EXPECT_EQ(ChunkingStatus::kUnknown, record.chunking_status);
    ParseMessageBodyChunked(&record);
    EXPECT_EQ(ChunkingStatus::kComplete, record.chunking_status);
    EXPECT_EQ("PixieLabs", record.http_resp_body);
  }
  {
    HTTPTraceRecord record;
    record.http_resp_body = "9\r\nPixieLabs\r\n";
    EXPECT_EQ(ChunkingStatus::kUnknown, record.chunking_status);
    ParseMessageBodyChunked(&record);
    EXPECT_EQ(ChunkingStatus::kChunked, record.chunking_status);
    EXPECT_EQ("PixieLabs", record.http_resp_body);
  }
  {
    HTTPTraceRecord record;
    record.http_resp_body = "9\r\nPixie";
    EXPECT_EQ(ChunkingStatus::kUnknown, record.chunking_status);
    ParseMessageBodyChunked(&record);
    EXPECT_EQ(ChunkingStatus::kChunked, record.chunking_status);
    EXPECT_EQ("Pixie", record.http_resp_body) << "Parse failed, data is not resized";
  }
}

TEST(PreProcessRecordTest, GzipCompressedContentIsDecompressed) {
  HTTPTraceRecord record;
  record.http_headers[http_headers::kContentEncoding] = "gzip";
  const uint8_t compressed_bytes[] = {0x1f, 0x8b, 0x08, 0x00, 0x37, 0xf0, 0xbf, 0x5c, 0x00,
                                      0x03, 0x0b, 0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44,
                                      0x85, 0x92, 0xd4, 0xe2, 0x12, 0x2e, 0x00, 0x8c, 0x2d,
                                      0xc0, 0xfa, 0x0f, 0x00, 0x00, 0x00};
  record.http_resp_body.assign(reinterpret_cast<const char*>(compressed_bytes),
                               sizeof(compressed_bytes));
  PreProcessRecord(&record);
  EXPECT_EQ("This is a test\n", record.http_resp_body);
}

TEST(PreProcessRecordTest, ContentHeaderIsNotAdded) {
  HTTPTraceRecord record;
  record.http_resp_body = "test";
  PreProcessRecord(&record);
  EXPECT_EQ("test", record.http_resp_body);
  EXPECT_THAT(record.http_headers, Not(Contains(Key(http_headers::kContentEncoding))));
}

TEST(ParseEventAttrTest, DataIsCopied) {
  socket_data_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.fd = 3;

  HTTPTraceRecord record;

  ParseEventAttr(event, &record);

  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(3, record.fd);
}

TEST(ParseHTTPRequestTest, RequestIsIdentified) {
  const std::string http_request = R"(GET /index.html HTTP/1.1
Host: www.pixielabs.ai

<body>)";
  socket_data_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.fd = 3;
  event.attr.msg_size = http_request.size();
  http_request.copy(event.msg, http_request.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseHTTPRequest(event, &record));
  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(3, record.fd);
  EXPECT_EQ(SocketTraceEventType::kHTTPRequest, record.event_type);
  EXPECT_THAT(record.http_headers, ElementsAre(Pair("Host", "www.pixielabs.ai")));
  EXPECT_EQ(1, record.http_minor_version);
  EXPECT_EQ("GET", record.http_req_method);
  EXPECT_EQ("/index.html", record.http_req_path);
}

TEST(ParseHTTPResponseTest, ResponseIsIdentified) {
  const std::string http_response = R"(HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8

pixielabs)";

  socket_data_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.fd = 3;
  event.attr.msg_size = http_response.size();
  http_response.copy(event.msg, http_response.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseHTTPResponse(event, &record));
  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(3, record.fd);
  EXPECT_EQ(SocketTraceEventType::kHTTPResponse, record.event_type);
  EXPECT_THAT(record.http_headers,
              ElementsAre(Pair("Content-Type", "application/json; charset=utf-8")));
  EXPECT_EQ(1, record.http_minor_version);
  EXPECT_EQ("OK", record.http_resp_message);
  EXPECT_EQ("pixielabs", record.http_resp_body);
}

TEST(ParseRawTest, ContentIsCopied) {
  const std::string data = "test";
  socket_data_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.fd = 3;
  event.attr.msg_size = data.size();
  data.copy(event.msg, data.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseRaw(event, &record));
  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(3, record.fd);
  EXPECT_EQ(SocketTraceEventType::kUnknown, record.event_type);
  EXPECT_EQ("test", record.http_resp_body);
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
  return lhs.is_complete == rhs.is_complete && lhs.type == rhs.type &&
         lhs.http_minor_version == rhs.http_minor_version && lhs.http_headers == rhs.http_headers &&
         lhs.http_req_method == rhs.http_req_method && lhs.http_req_path == rhs.http_req_path &&
         lhs.http_req_body == rhs.http_req_body && lhs.http_resp_status == rhs.http_resp_status &&
         lhs.http_resp_message == rhs.http_resp_message && lhs.http_resp_body == rhs.http_resp_body;
}

TEST_F(HTTPParserTest, ParseCompleteHTTPResponseWithContentLengthHeader) {
  const std::string_view msg1 = R"(HTTP/1.1 200 OK
Content-Type: foo
Content-Length: 9

pixielabs)";
  const std::string_view msg2 = R"(HTTP/1.1 200 OK
Content-Type: bar
Content-Length: 10

pixielabs!)";
  EXPECT_EQ(HTTPParser::ParseState::kSuccess, parser_.ParseResponse(0, msg1));
  EXPECT_EQ(HTTPParser::ParseState::kSuccess, parser_.ParseResponse(1, msg2));

  HTTPMessage expected_message1;
  expected_message1.is_complete = true;
  expected_message1.type = SocketTraceEventType::kHTTPResponse;
  expected_message1.http_minor_version = 1;
  expected_message1.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "9"}};
  expected_message1.http_resp_status = 200;
  expected_message1.http_resp_message = "OK";
  expected_message1.http_resp_body = "pixielabs";

  HTTPMessage expected_message2;
  expected_message2.is_complete = true;
  expected_message2.type = SocketTraceEventType::kHTTPResponse;
  expected_message2.http_minor_version = 1;
  expected_message2.http_headers = {{"Content-Type", "bar"}, {"Content-Length", "10"}};
  expected_message2.http_resp_status = 200;
  expected_message2.http_resp_message = "OK";
  expected_message2.http_resp_body = "pixielabs!";

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

  EXPECT_EQ(HTTPParser::ParseState::kNeedsMoreData, parser_.ParseResponse(0, msg1));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  EXPECT_EQ(HTTPParser::ParseState::kNeedsMoreData, parser_.ParseResponse(1, msg2));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  EXPECT_EQ(HTTPParser::ParseState::kSuccess, parser_.ParseResponse(2, msg3));

  HTTPMessage expected_message1;
  expected_message1.is_complete = true;
  expected_message1.type = SocketTraceEventType::kHTTPResponse;
  expected_message1.http_minor_version = 1;
  expected_message1.http_headers = {{"Content-Type", "foo"}, {"Content-Length", "21"}};
  expected_message1.http_resp_status = 200;
  expected_message1.http_resp_message = "OK";
  expected_message1.http_resp_body = "pixielabs is awesome!";

  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message1));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
}

TEST_F(HTTPParserTest, InvalidInput) {
  EXPECT_EQ(HTTPParser::ParseState::kInvalid, parser_.ParseResponse(0, "test"));
  EXPECT_EQ(HTTPParser::ParseState::kUnknown, parser_.ParseResponse(2, "test"));
}

// Leave http_resp_body set by caller.
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
  expected_message.http_resp_body = "pixielabs is awesome!";

  EXPECT_EQ(HTTPParser::ParseState::kSuccess, parser_.ParseResponse(0, msg));
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
  expected_message.http_resp_body = "pixielabs is awesome!";

  EXPECT_EQ(HTTPParser::ParseState::kNeedsMoreData, parser_.ParseResponse(0, msg1));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  EXPECT_EQ(HTTPParser::ParseState::kNeedsMoreData, parser_.ParseResponse(1, msg2));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  EXPECT_EQ(HTTPParser::ParseState::kSuccess, parser_.ParseResponse(2, msg3));
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
  expected_message.http_resp_body = "pixielabs";

  EXPECT_EQ(HTTPParser::ParseState::kNeedsMoreData, parser_.ParseResponse(0, msg1));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  EXPECT_EQ(HTTPParser::ParseState::kNeedsMoreData, parser_.ParseResponse(1, msg2));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty());
  EXPECT_EQ(HTTPParser::ParseState::kSuccess, parser_.ParseResponse(2, msg3));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), ElementsAre(expected_message));
  EXPECT_THAT(parser_.ExtractHTTPMessages(), IsEmpty()) << "Data should be empty after extraction";
}

}  // namespace stirling
}  // namespace pl
