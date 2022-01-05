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

#include <utility>
#include <vector>
#include "src/common/base/types.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/decoder/packet_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

bool operator==(const SyncGroupAssignment& lhs, const SyncGroupAssignment& rhs) {
  return lhs.member_id == rhs.member_id;
}

bool operator!=(const SyncGroupAssignment& lhs, const SyncGroupAssignment& rhs) {
  return !(lhs == rhs);
}

bool operator==(const SyncGroupReq& lhs, const SyncGroupReq& rhs) {
  if (lhs.group_id != rhs.group_id) {
    return false;
  }
  if (lhs.generation_id != rhs.generation_id) {
    return false;
  }
  if (lhs.member_id != rhs.member_id) {
    return false;
  }
  if (lhs.group_instance_id != rhs.group_instance_id) {
    return false;
  }
  if (lhs.protocol_type != rhs.protocol_type) {
    return false;
  }
  if (lhs.protocol_name != rhs.protocol_name) {
    return false;
  }
  for (size_t i = 0; i < lhs.assignments.size(); ++i) {
    if (lhs.assignments[i] != rhs.assignments[i]) {
      return false;
    }
  }
  return true;
}

bool operator==(const SyncGroupResp& lhs, const SyncGroupResp& rhs) {
  if (lhs.throttle_time_ms != rhs.throttle_time_ms) {
    return false;
  }
  if (lhs.error_code != rhs.error_code) {
    return false;
  }
  if (lhs.protocol_type != rhs.protocol_type) {
    return false;
  }
  return lhs.protocol_name == rhs.protocol_name;
}

TEST(KafkaPacketDecoderTest, ExtractSyncGroupReq) {
  const std::string_view input = CreateStringView<char>(
      "\x16\x63\x6f\x6e\x73\x6f\x6c\x65\x2d\x63\x6f\x6e\x73\x75\x6d\x65\x72\x2d\x33\x35\x34\x30\x00"
      "\x00\x00\x01\x46\x63\x6f\x6e\x73\x75\x6d\x65\x72\x2d\x63\x6f\x6e\x73\x6f\x6c\x65\x2d\x63\x6f"
      "\x6e\x73\x75\x6d\x65\x72\x2d\x33\x35\x34\x30\x2d\x31\x2d\x36\x35\x65\x38\x65\x32\x64\x61\x2d"
      "\x66\x65\x38\x38\x2d\x34\x64\x63\x61\x2d\x39\x30\x65\x33\x2d\x30\x62\x37\x30\x63\x39\x61\x62"
      "\x61\x37\x31\x61\x00\x09\x63\x6f\x6e\x73\x75\x6d\x65\x72\x06\x72\x61\x6e\x67\x65\x02\x46\x63"
      "\x6f\x6e\x73\x75\x6d\x65\x72\x2d\x63\x6f\x6e\x73\x6f\x6c\x65\x2d\x63\x6f\x6e\x73\x75\x6d\x65"
      "\x72\x2d\x33\x35\x34\x30\x2d\x31\x2d\x36\x35\x65\x38\x65\x32\x64\x61\x2d\x66\x65\x38\x38\x2d"
      "\x34\x64\x63\x61\x2d\x39\x30\x65\x33\x2d\x30\x62\x37\x30\x63\x39\x61\x62\x61\x37\x31\x61\x26"
      "\x00\x01\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65\x6e"
      "\x74\x73\x00\x00\x00\x01\x00\x00\x00\x00\xff\xff\xff\xff\x00\x00");
  SyncGroupReq expected_result{
      .group_id = "console-consumer-3540",
      .generation_id = 1,
      .member_id = "consumer-console-consumer-3540-1-65e8e2da-fe88-4dca-90e3-0b70c9aba71a",
      .group_instance_id = "",
      .protocol_type = "consumer",
      .protocol_name = "range",
      .assignments = {
          {.member_id = "consumer-console-consumer-3540-1-65e8e2da-fe88-4dca-90e3-0b70c9aba71a"}}};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kSyncGroup, 5);
  EXPECT_OK_AND_EQ(decoder.ExtractSyncGroupReq(), expected_result);
}

TEST(KafkaPacketDecoderTest, ExtractSyncGroupResp) {
  const std::string_view input = CreateStringView<char>(
      "\x00\x00\x00\x00\x00\x00\x09\x63\x6f\x6e\x73\x75\x6d\x65\x72\x06\x72\x61\x6e\x67\x65\x26\x00"
      "\x01\x00\x00\x00\x01\x00\x11\x71\x75\x69\x63\x6b\x73\x74\x61\x72\x74\x2d\x65\x76\x65\x6e\x74"
      "\x73\x00\x00\x00\x01\x00\x00\x00\x00\xff\xff\xff\xff\x00");
  SyncGroupResp expected_result{.throttle_time_ms = 0,
                                .error_code = 0,
                                .protocol_type = "consumer",
                                .protocol_name = "range"};
  PacketDecoder decoder(input);
  decoder.SetAPIInfo(APIKey::kSyncGroup, 5);
  EXPECT_OK_AND_EQ(decoder.ExtractSyncGroupResp(), expected_result);
}

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
