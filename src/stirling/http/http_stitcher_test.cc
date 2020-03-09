#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/http/http_stitcher.h"

namespace pl {
namespace stirling {
namespace http {

using ::testing::Contains;
using ::testing::Pair;

TEST(PreProcessRecordTest, GzipCompressedContentIsDecompressed) {
  Message message;
  message.type = MessageType::kResponse;
  message.http_headers.insert({kContentEncoding, "gzip"});
  // Not really json, but specify json so the content is not ignored.
  message.http_headers.insert({kContentType, "json"});
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
  Message message;
  message.type = MessageType::kResponse;
  message.http_msg_body = "test";
  message.http_headers.insert({kContentType, "text"});
  PreProcessMessage(&message);
  EXPECT_EQ("<removed>", message.http_msg_body);
  EXPECT_THAT(message.http_headers, Contains(Pair(kContentType, "text")));
}

}  // namespace http
}  // namespace stirling
}  // namespace pl
