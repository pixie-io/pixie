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

#include "src/stirling/source_connectors/socket_tracer/protocols/tls/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/tls/parse.h"

namespace px {
namespace stirling {
namespace protocols {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

class StitchFramesTest : public ::testing::Test {};

tls::Frame CreateTLSFrame(uint64_t ts_ns, tls::ContentType content_type,
                          tls::LegacyVersion legacy_version, tls::HandshakeType handshake_type,
                          std::string session_id) {
  tls::Frame frame;
  frame.timestamp_ns = ts_ns;
  frame.content_type = content_type;
  frame.legacy_version = legacy_version;
  frame.handshake_type = handshake_type;
  frame.session_id = session_id;
  return frame;
}

tls::Frame CreateNonHandshakeFrame(uint64_t ts_ns, tls::ContentType content_type,
                                   tls::LegacyVersion legacy_version) {
  tls::Frame frame;
  frame.timestamp_ns = ts_ns;
  frame.content_type = content_type;
  frame.legacy_version = legacy_version;
  return frame;
}

TEST_F(StitchFramesTest, HandlesTLS1_3Handshake) {
  std::deque<tls::Frame> reqs = {
      CreateTLSFrame(0, tls::ContentType::kHandshake, tls::LegacyVersion::kTLS1_0,
                     tls::HandshakeType::kClientHello, "session_id"),
  };
  std::deque<tls::Frame> resps = {
      CreateTLSFrame(1, tls::ContentType::kHandshake, tls::LegacyVersion::kTLS1_2,
                     tls::HandshakeType::kServerHello, "session_id"),
  };
  RecordsWithErrorCount<tls::Record> result = tls::StitchFrames(&reqs, &resps);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, HandlesApplicationDataAndChangeCipherSpecWithFullHandshake) {
  std::deque<tls::Frame> reqs = {
      CreateTLSFrame(0, tls::ContentType::kHandshake, tls::LegacyVersion::kTLS1_0,
                     tls::HandshakeType::kClientHello, "session_id"),
      CreateNonHandshakeFrame(2, tls::ContentType::kChangeCipherSpec, tls::LegacyVersion::kTLS1_2),
      CreateNonHandshakeFrame(3, tls::ContentType::kApplicationData, tls::LegacyVersion::kTLS1_2),
      CreateNonHandshakeFrame(4, tls::ContentType::kApplicationData, tls::LegacyVersion::kTLS1_2),
  };
  std::deque<tls::Frame> resps = {
      CreateTLSFrame(1, tls::ContentType::kHandshake, tls::LegacyVersion::kTLS1_2,
                     tls::HandshakeType::kServerHello, "session_id"),
      CreateNonHandshakeFrame(3, tls::ContentType::kChangeCipherSpec, tls::LegacyVersion::kTLS1_2),
      CreateNonHandshakeFrame(5, tls::ContentType::kApplicationData, tls::LegacyVersion::kTLS1_2),
  };
  RecordsWithErrorCount<tls::Record> result = tls::StitchFrames(&reqs, &resps);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);
  EXPECT_THAT(reqs, IsEmpty());
  EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, WaitsToProcessReqFrameUntilRespFrameIsReceived) {
  std::deque<tls::Frame> reqs = {
      CreateTLSFrame(0, tls::ContentType::kHandshake, tls::LegacyVersion::kTLS1_0,
                     tls::HandshakeType::kClientHello, "session_id"),
      CreateNonHandshakeFrame(2, tls::ContentType::kChangeCipherSpec, tls::LegacyVersion::kTLS1_2),
  };
  std::deque<tls::Frame> resps = {
      CreateTLSFrame(1, tls::ContentType::kHandshake, tls::LegacyVersion::kTLS1_2,
                     tls::HandshakeType::kServerHello, "session_id"),
  };
  RecordsWithErrorCount<tls::Record> result = tls::StitchFrames(&reqs, &resps);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);
  EXPECT_THAT(reqs, SizeIs(1));
  EXPECT_THAT(resps, IsEmpty());
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
