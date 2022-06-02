/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/http/stitcher.h"

namespace px {
namespace stirling {
namespace protocols {
namespace http {

using ::testing::Contains;
using ::testing::Pair;
using ::testing::StrEq;

TEST(PreProcessRecordTest, GzipCompressedContentIsDecompressed) {
  Message message;
  message.type = message_type_t::kResponse;
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
  message.type = message_type_t::kResponse;
  message.body = "test";
  message.headers.insert({kContentType, "text"});
  PreProcessMessage(&message);
  EXPECT_EQ("<removed: non-text content-type>", message.body);
  EXPECT_THAT(message.headers, Contains(Pair(kContentType, "text")));
}

// Tests that when body-size is 0, the message body won't be rewritten.
TEST(PreProcessRecordTest, ZeroSizedBodyNotRewritten) {
  Message message;
  message.type = message_type_t::kResponse;
  message.body_size = 0;
  EXPECT_THAT(message.body, StrEq("-"));
  PreProcessMessage(&message);
  EXPECT_THAT(message.body, StrEq("-"));
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
