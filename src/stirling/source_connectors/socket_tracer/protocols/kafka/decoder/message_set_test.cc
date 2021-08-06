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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/opcodes/message_set.h"
#include <utility>
#include <vector>
#include "src/common/base/types.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/decoder/packet_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::px::operator<<;

TEST(KafkaPacketDecoderTest, ExtractRecordMessage) {
  // Empty key and value Record.
  {
    std::string_view input = CreateStringView<char>("\x0c\x00\x00\x00\x01\x00\x00");
    RecordMessage expected_result{};
    PacketDecoder decoder(input);
    EXPECT_OK_AND_EQ(decoder.ExtractRecordMessage(), expected_result);
  }
  {
    std::string_view input =
        CreateStringView<char>("\x28\x00\x00\x00\x06key\x1cMy first event\x00");
    RecordMessage expected_result{.key = "key", .value = "My first event"};
    PacketDecoder decoder(input);
    EXPECT_OK_AND_EQ(decoder.ExtractRecordMessage(), expected_result);
  }
}

TEST(KafkaPacketDecoderTest, ExtractRecordBatchV8) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x46\xff\xff\xff\xff\x02\xa7\x88\x71\xd8\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01\x7a\xb2\x0a\x70\x1d\x00\x00\x01\x7a\xb2\x0a\x70\x1d\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x28\x00\x00\x00\x01"
      "\x1c\x4d\x79\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e\x74\x00");
  RecordBatch expected_result{{{.key = "", .value = "My first event"}}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 8);
  EXPECT_OK_AND_EQ(decoder.ExtractRecordBatch(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractRecordBatchV9) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x4e\xff\xff\xff\xff\x02\xc0\xde\x91\x11\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x01\x7a\x1b\xc8\x2d\xaa\x00\x00\x01\x7a\x1b\xc8\x2d\xaa\xff"
      "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x01\x38\x00\x00\x00\x01"
      "\x2c\x54\x68\x69\x73\x20\x69\x73\x20\x6d\x79\x20\x66\x69\x72\x73\x74\x20\x65\x76\x65\x6e"
      "\x74\x00");
  RecordBatch expected_result{{{.key = "", .value = "This is my first event"}}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kProduce, 9);
  EXPECT_OK_AND_EQ(decoder.ExtractRecordBatch(), expected_result);
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
