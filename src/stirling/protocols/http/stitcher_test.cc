#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/protocols/http/stitcher.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http {

using ::testing::Contains;
using ::testing::Pair;

TEST(PreProcessRecordTest, GzipCompressedContentIsDecompressed) {
  Message message;
  message.type = MessageType::kResponse;
  message.headers.insert({kContentEncoding, "gzip"});
  // Not really json, but specify json so the content is not ignored.
  message.headers.insert({kContentType, "json"});
  const uint8_t compressed_bytes[] = {0x1f, 0x8b, 0x08, 0x00, 0x37, 0xf0, 0xbf, 0x5c, 0x00,
                                      0x03, 0x0b, 0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44,
                                      0x85, 0x92, 0xd4, 0xe2, 0x12, 0x2e, 0x00, 0x8c, 0x2d,
                                      0xc0, 0xfa, 0x0f, 0x00, 0x00, 0x00};
  message.body.assign(reinterpret_cast<const char*>(compressed_bytes), sizeof(compressed_bytes));
  PreProcessMessage(&message);
  EXPECT_EQ("This is a test\n", message.body);
}

TEST(PreProcessRecordTest, ContentHeaderIsNotAdded) {
  Message message;
  message.type = MessageType::kResponse;
  message.body = "test";
  message.headers.insert({kContentType, "text"});
  PreProcessMessage(&message);
  EXPECT_EQ("<removed: non-text content-type>", message.body);
  EXPECT_THAT(message.headers, Contains(Pair(kContentType, "text")));
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
