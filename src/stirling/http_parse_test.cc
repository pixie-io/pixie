#include <gtest/gtest.h>

#include "src/stirling/http_parse.h"

namespace pl {
namespace stirling {

TEST(ParseChunkedMessageBodyTest, DetectChunkedMessage) {
  {
    HTTPTraceRecord record;
    record.http_resp_body = "9\r\nPixieLabs\r\n0\r\n\r\n";
    EXPECT_EQ(ChunkingStatus::UNSPECIFIED, record.chunking_status);
    ParseMessageBodyChunked(&record);
    EXPECT_EQ(ChunkingStatus::COMPLETE, record.chunking_status);
    EXPECT_EQ("PixieLabs", record.http_resp_body);
  }
  {
    HTTPTraceRecord record;
    record.http_resp_body = "9\r\nPixieLabs\r\n";
    EXPECT_EQ(ChunkingStatus::UNSPECIFIED, record.chunking_status);
    ParseMessageBodyChunked(&record);
    EXPECT_EQ(ChunkingStatus::CHUNKED, record.chunking_status);
    EXPECT_EQ("PixieLabs", record.http_resp_body);
  }
  {
    HTTPTraceRecord record;
    record.http_resp_body = "9\r\nPixie";
    EXPECT_EQ(ChunkingStatus::UNSPECIFIED, record.chunking_status);
    ParseMessageBodyChunked(&record);
    EXPECT_EQ(ChunkingStatus::CHUNKED, record.chunking_status);
    EXPECT_EQ("Pixie", record.http_resp_body) << "Parse failed, data is not resized";
  }
}

}  // namespace stirling
}  // namespace pl
