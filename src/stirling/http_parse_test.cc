#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/http_parse.h"

namespace pl {
namespace stirling {

using ::testing::Contains;
using ::testing::ElementsAre;
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
  record.http_headers[http_header_keys::kContentEncoding] = "gzip";
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
  EXPECT_THAT(record.http_headers, Not(Contains(Key(http_header_keys::kContentEncoding))));
}

TEST(ParseEventAttrTest, DataIsCopied) {
  syscall_write_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.pid = 2;
  event.attr.fd = 3;

  HTTPTraceRecord record;

  ParseEventAttr(event, &record);

  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(2, record.pid);
  EXPECT_EQ(3, record.fd);
}

TEST(ParseHTTPRequestTest, RequestIsIdentified) {
  const std::string http_request = R"(GET /index.html HTTP/1.1
Host: www.pixielabs.ai

<body>)";
  syscall_write_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.pid = 2;
  event.attr.fd = 3;
  event.attr.msg_buf_size = sizeof(event.msg);
  event.attr.msg_bytes = http_request.size();
  http_request.copy(event.msg, http_request.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseHTTPRequest(event, &record, http_request.size()));
  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(2, record.pid);
  EXPECT_EQ(3, record.fd);
  EXPECT_EQ("http_request", record.event_type);
  EXPECT_THAT(record.http_headers, ElementsAre(Pair("Host", "www.pixielabs.ai")));
  EXPECT_EQ(1, record.http_minor_version);
  EXPECT_EQ("GET", record.http_req_method);
  EXPECT_EQ("/index.html", record.http_req_path);
}

TEST(ParseHTTPResponseTest, ResponseIsIdentified) {
  const std::string http_response = R"(HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8

pixielabs)";

  syscall_write_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.pid = 2;
  event.attr.fd = 3;
  event.attr.msg_buf_size = sizeof(event.msg);
  event.attr.msg_bytes = http_response.size();
  http_response.copy(event.msg, http_response.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseHTTPResponse(event, &record, http_response.size()));
  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(2, record.pid);
  EXPECT_EQ(3, record.fd);
  EXPECT_EQ("http_response", record.event_type);
  EXPECT_THAT(record.http_headers,
              ElementsAre(Pair("Content-Type", "application/json; charset=utf-8")));
  EXPECT_EQ(1, record.http_minor_version);
  EXPECT_EQ("OK", record.http_resp_message);
  EXPECT_EQ("pixielabs", record.http_resp_body);
}

TEST(ParseRawTest, ContentIsCopied) {
  const std::string data = "test";
  syscall_write_event_t event;
  event.attr.time_stamp_ns = 100;
  event.attr.tgid = 1;
  event.attr.pid = 2;
  event.attr.fd = 3;
  event.attr.msg_buf_size = sizeof(event.msg);
  event.attr.msg_bytes = data.size();
  data.copy(event.msg, data.size());

  HTTPTraceRecord record;

  EXPECT_TRUE(ParseRaw(event, &record, data.size()));
  EXPECT_EQ(100, record.time_stamp_ns);
  EXPECT_EQ(1, record.tgid);
  EXPECT_EQ(2, record.pid);
  EXPECT_EQ(3, record.fd);
  EXPECT_EQ("parse_failure", record.event_type);
  EXPECT_EQ("test", record.http_resp_body);
}

}  // namespace stirling
}  // namespace pl
