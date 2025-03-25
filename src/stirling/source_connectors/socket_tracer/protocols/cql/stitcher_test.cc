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

#include <absl/container/flat_hash_map.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/test_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/test_utils.h"

using ::testing::HasSubstr;
using ::testing::IsEmpty;

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

using testutils::CreateFrame;
//-----------------------------------------------------------------------------
// Test Data
//-----------------------------------------------------------------------------

// STARTUP request from client establishing connection.
// Contains CQL_VERSION: 3.0.0
constexpr uint8_t kStartupReq[] = {0x00, 0x01, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
                                   0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00,
                                   0x05, 0x33, 0x2e, 0x30, 0x2e, 0x30};

// AUTHENTICATE response, asking for authentication in response to STARTUP.
// Contains: org.apache.cassandra.auth.PasswordAuthenticator
constexpr uint8_t kAuthenticateResp[] = {0x00, 0x2f, 0x6f, 0x72, 0x67, 0x2e, 0x61, 0x70, 0x61, 0x63,
                                         0x68, 0x65, 0x2e, 0x63, 0x61, 0x73, 0x73, 0x61, 0x6e, 0x64,
                                         0x72, 0x61, 0x2e, 0x61, 0x75, 0x74, 0x68, 0x2e, 0x50, 0x61,
                                         0x73, 0x73, 0x77, 0x6f, 0x72, 0x64, 0x41, 0x75, 0x74, 0x68,
                                         0x65, 0x6e, 0x74, 0x69, 0x63, 0x61, 0x74, 0x6f, 0x72};

// AUTH_RESPONSE request from client, in response to an AUTHENTICATE response from server.
// Contains username/password of cassandra/cql.
constexpr uint8_t kAuthResponseReq[] = {0x00, 0x00, 0x00, 0x14, 0x00, 0x63, 0x61, 0x73,
                                        0x73, 0x61, 0x6e, 0x64, 0x72, 0x61, 0x00, 0x63,
                                        0x61, 0x73, 0x73, 0x61, 0x6e, 0x64, 0x72, 0x61};

// AUTH_SUCCESS response from server. Sent after successful AUTHENTICATE request.
// Content is null pointer.
constexpr uint8_t kAuthSuccessResp[] = {0xff, 0xff, 0xff, 0xff};

// REGISTER request from client, asking to register for certain events.
// This particular one requests notification on TOPOLOGY_CHANGE, STATUS_CHANGE or SCHEMA_CHANGE.
constexpr uint8_t kRegisterReq[] = {0x00, 0x03, 0x00, 0x0f, 0x54, 0x4f, 0x50, 0x4f, 0x4c, 0x4f,
                                    0x47, 0x59, 0x5f, 0x43, 0x48, 0x41, 0x4e, 0x47, 0x45, 0x00,
                                    0x0d, 0x53, 0x54, 0x41, 0x54, 0x55, 0x53, 0x5f, 0x43, 0x48,
                                    0x41, 0x4e, 0x47, 0x45, 0x00, 0x0d, 0x53, 0x43, 0x48, 0x45,
                                    0x4d, 0x41, 0x5f, 0x43, 0x48, 0x41, 0x4e, 0x47, 0x45};

// READY response from server, after STARTUP, AUTH_SUCCESS or REGISTER.
// Body is empty.
// constexpr uint8_t kReadyResp[] = {};

// QUERY request from client.
// Contains: SELECT * FROM system.peers
constexpr uint8_t kQueryReq[] = {0x00, 0x00, 0x00, 0x1a, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20,
                                 0x2a, 0x20, 0x46, 0x52, 0x4f, 0x4d, 0x20, 0x73, 0x79, 0x73, 0x74,
                                 0x65, 0x6d, 0x2e, 0x70, 0x65, 0x65, 0x72, 0x73, 0x00, 0x01, 0x00};

// RESULT response to query kQueryReq above.
// Result contains 9 columns, and 0 rows. Columns are:
// peer,data_center,host_id,preferred_ip,rack,release_version,rpc_address,schema_version,tokens
constexpr uint8_t kResultResp[] = {
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00, 0x06, 0x73, 0x79,
    0x73, 0x74, 0x65, 0x6d, 0x00, 0x05, 0x70, 0x65, 0x65, 0x72, 0x73, 0x00, 0x04, 0x70, 0x65, 0x65,
    0x72, 0x00, 0x10, 0x00, 0x0b, 0x64, 0x61, 0x74, 0x61, 0x5f, 0x63, 0x65, 0x6e, 0x74, 0x65, 0x72,
    0x00, 0x0d, 0x00, 0x07, 0x68, 0x6f, 0x73, 0x74, 0x5f, 0x69, 0x64, 0x00, 0x0c, 0x00, 0x0c, 0x70,
    0x72, 0x65, 0x66, 0x65, 0x72, 0x72, 0x65, 0x64, 0x5f, 0x69, 0x70, 0x00, 0x10, 0x00, 0x04, 0x72,
    0x61, 0x63, 0x6b, 0x00, 0x0d, 0x00, 0x0f, 0x72, 0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x5f, 0x76,
    0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0d, 0x00, 0x0b, 0x72, 0x70, 0x63, 0x5f, 0x61, 0x64,
    0x64, 0x72, 0x65, 0x73, 0x73, 0x00, 0x10, 0x00, 0x0e, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x5f,
    0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x0c, 0x00, 0x06, 0x74, 0x6f, 0x6b, 0x65, 0x6e,
    0x73, 0x00, 0x22, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00};

// QUERY request from client, that that produces an error.
// Contains: SELECT * FROM system.schema_keyspaces ;
constexpr uint8_t kBadQueryReq[] = {
    0x00, 0x00, 0x00, 0x27, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x2a, 0x20, 0x46, 0x52,
    0x4f, 0x4d, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e, 0x73, 0x63, 0x68, 0x65, 0x6d,
    0x61, 0x5f, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73, 0x20, 0x3b, 0x00, 0x01,
    0x34, 0x00, 0x00, 0x00, 0x64, 0x00, 0x08, 0x00, 0x05, 0x9d, 0xaf, 0x91, 0xd4, 0xc0, 0x5c};

// ERROR response from server to kBadQueryReq.
// Error message contains: unconfigured table schema_keyspaces
constexpr uint8_t kBadQueryErrorResp[] = {
    0x00, 0x00, 0x22, 0x00, 0x00, 0x23, 0x75, 0x6e, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67,
    0x75, 0x72, 0x65, 0x64, 0x20, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x73, 0x63, 0x68,
    0x65, 0x6d, 0x61, 0x5f, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73};

// PREPARE request from client.
// UPDATE counter1 SET "C0"="C0"+?,"C1"="C1"+?,"C2"="C2"+?,"C3"="C3"+?,"C4"="C4"+? WHERE KEY=?
constexpr uint8_t kPrepareReq[] = {
    0x00, 0x00, 0x00, 0x5b, 0x55, 0x50, 0x44, 0x41, 0x54, 0x45, 0x20, 0x63, 0x6f, 0x75, 0x6e, 0x74,
    0x65, 0x72, 0x31, 0x20, 0x53, 0x45, 0x54, 0x20, 0x22, 0x43, 0x30, 0x22, 0x3d, 0x22, 0x43, 0x30,
    0x22, 0x2b, 0x3f, 0x2c, 0x22, 0x43, 0x31, 0x22, 0x3d, 0x22, 0x43, 0x31, 0x22, 0x2b, 0x3f, 0x2c,
    0x22, 0x43, 0x32, 0x22, 0x3d, 0x22, 0x43, 0x32, 0x22, 0x2b, 0x3f, 0x2c, 0x22, 0x43, 0x33, 0x22,
    0x3d, 0x22, 0x43, 0x33, 0x22, 0x2b, 0x3f, 0x2c, 0x22, 0x43, 0x34, 0x22, 0x3d, 0x22, 0x43, 0x34,
    0x22, 0x2b, 0x3f, 0x20, 0x57, 0x48, 0x45, 0x52, 0x45, 0x20, 0x4b, 0x45, 0x59, 0x3d, 0x3f};

// RESULT response from server to kPrepareReq above.
// Response specifies itself as a PREPARED kind, and contains an ID and some metadata.
constexpr uint8_t kPrepareResultResp[] = {
    0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x5c, 0x6e, 0x15, 0xd4, 0x08, 0x4b, 0x0f, 0xd0, 0xd5,
    0x5a, 0x6e, 0x4b, 0x16, 0x48, 0x92, 0x7c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x00, 0x09, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63,
    0x65, 0x31, 0x00, 0x08, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x65, 0x72, 0x31, 0x00, 0x02, 0x43,
    0x30, 0x00, 0x05, 0x00, 0x02, 0x43, 0x31, 0x00, 0x05, 0x00, 0x02, 0x43, 0x32, 0x00, 0x05,
    0x00, 0x02, 0x43, 0x33, 0x00, 0x05, 0x00, 0x02, 0x43, 0x34, 0x00, 0x05, 0x00, 0x03, 0x6b,
    0x65, 0x79, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};

// EXECUTE request from client, linked to kPrepareReq.
// C0-C4 are set to 1, while KEY is set to 3639334E3732504E3930
constexpr uint8_t kExecuteReq[] = {
    0x00, 0x10, 0x5c, 0x6e, 0x15, 0xd4, 0x08, 0x4b, 0x0f, 0xd0, 0xd5, 0x5a, 0x6e, 0x4b, 0x16, 0x48,
    0x92, 0x7c, 0x00, 0x0a, 0x25, 0x00, 0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x36, 0x39, 0x33, 0x4e, 0x37, 0x32, 0x50, 0x4e, 0x39,
    0x30, 0x00, 0x00, 0x13, 0x88, 0x00, 0x05, 0x9e, 0x07, 0x8a, 0x50, 0xb9, 0x00};

// RESULT response to kPrepareReq.
// Result type is VOID, since it was an UPDATE request.
constexpr uint8_t kExecuteResultResp[] = {0x00, 0x00, 0x00, 0x01};

// OPTIONS request from client, asking server what options it supports.
// Body is empty.
// constexpr uint8_t kOptionsReq[] = {};

// SUPPORTED response from server, after an OPTIONS request.
// This particular one contains:
//   "COMPRESSION":["snappy","lz4"]
//   "CQL_VERSION":["3.4.4"]
//   "PROTOCOL_VERSIONS":["3/v3","4/v4","5/v5-beta"]}
constexpr uint8_t kSupportedResp[] = {
    0x00, 0x03, 0x00, 0x11, 0x50, 0x52, 0x4f, 0x54, 0x4f, 0x43, 0x4f, 0x4c, 0x5f, 0x56, 0x45, 0x52,
    0x53, 0x49, 0x4f, 0x4e, 0x53, 0x00, 0x03, 0x00, 0x04, 0x33, 0x2f, 0x76, 0x33, 0x00, 0x04, 0x34,
    0x2f, 0x76, 0x34, 0x00, 0x09, 0x35, 0x2f, 0x76, 0x35, 0x2d, 0x62, 0x65, 0x74, 0x61, 0x00, 0x0b,
    0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x49, 0x4f, 0x4e, 0x00, 0x02, 0x00, 0x06, 0x73,
    0x6e, 0x61, 0x70, 0x70, 0x79, 0x00, 0x03, 0x6c, 0x7a, 0x34, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
    0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00, 0x01, 0x00, 0x05, 0x33, 0x2e, 0x34, 0x2e, 0x34};

// Asynchronous EVENT response from server. Content: SCHEMA_CHANGE DROPPED TABLE tutorialspoint emp
constexpr uint8_t kEventResp[] = {0x00, 0x0d, 0x53, 0x43, 0x48, 0x45, 0x4d, 0x41, 0x5f, 0x43, 0x48,
                                  0x41, 0x4e, 0x47, 0x45, 0x00, 0x07, 0x44, 0x52, 0x4f, 0x50, 0x50,
                                  0x45, 0x44, 0x00, 0x05, 0x54, 0x41, 0x42, 0x4c, 0x45, 0x00, 0x0e,
                                  0x74, 0x75, 0x74, 0x6f, 0x72, 0x69, 0x61, 0x6c, 0x73, 0x70, 0x6f,
                                  0x69, 0x6e, 0x74, 0x00, 0x03, 0x65, 0x6d, 0x70};

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

TEST(CassStitcherTest, OutOfOrderMatchingWithMissingResponses) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  int t = 0;

  Frame req0_s0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp0_s0_frame = CreateFrame(0, Opcode::kError, kBadQueryErrorResp, ++t);

  Frame req0_s1_frame = CreateFrame(1, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp0_s1_frame = CreateFrame(1, Opcode::kError, kBadQueryErrorResp, ++t);
  Frame req0_s2_frame = CreateFrame(2, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp0_s2_frame = CreateFrame(2, Opcode::kError, kBadQueryErrorResp, ++t);

  Frame req1_s0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryReq, ++t);
  Frame req1_s1_frame = CreateFrame(1, Opcode::kQuery, kBadQueryReq, ++t);
  Frame req1_s2_frame = CreateFrame(2, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp1_s1_frame = CreateFrame(1, Opcode::kError, kBadQueryErrorResp, ++t);
  Frame resp1_s2_frame = CreateFrame(2, Opcode::kError, kBadQueryErrorResp, ++t);

  Frame req2_s0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp2_s0_frame = CreateFrame(0, Opcode::kError, kBadQueryErrorResp, ++t);

  Frame req3_s0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryReq, ++t);
  Frame req4_s0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryReq, ++t);

  Frame req5_s0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp5_s0_frame = CreateFrame(0, Opcode::kError, kBadQueryErrorResp, ++t);

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  // create deque for stream0 on the stack
  req_map[0].push_back(req0_s0_frame);
  req_map[1].push_back(req0_s1_frame);
  req_map[2].push_back(req0_s2_frame);

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 3);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  req_map[0].push_back(req1_s0_frame);
  req_map[1].push_back(req1_s1_frame);
  req_map[2].push_back(req1_s2_frame);
  req_map[0].push_back(req2_s0_frame);
  req_map[0].push_back(req3_s0_frame);
  req_map[0].push_back(req4_s0_frame);
  req_map[0].push_back(req5_s0_frame);

  resp_map[0].push_back(resp0_s0_frame);
  resp_map[1].push_back(resp0_s1_frame);
  resp_map[2].push_back(resp0_s2_frame);
  resp_map[0].push_back(resp2_s0_frame);
  resp_map[0].push_back(resp5_s0_frame);

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 2);
  EXPECT_EQ(req_map[1].front().timestamp_ns, req1_s1_frame.timestamp_ns);
  EXPECT_EQ(req_map[2].front().timestamp_ns, req1_s2_frame.timestamp_ns);
  EXPECT_EQ(result.error_count, 3);
  EXPECT_EQ(result.records.size(), 5);

  // No requests or responses should be deleted when streams of
  // the head of requests are inactive
  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 2);
  EXPECT_EQ(req_map[1].front().timestamp_ns, req1_s1_frame.timestamp_ns);
  EXPECT_EQ(req_map[2].front().timestamp_ns, req1_s2_frame.timestamp_ns);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  resp_map[1].push_back(resp1_s1_frame);
  resp_map[2].push_back(resp1_s2_frame);

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 2);
}

// To test that, if a request of a response is missing, then the response is popped off.
TEST(CassStitcherTest, MissingRequest) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  int t = 0;
  Frame req1_frame = CreateFrame(1, Opcode::kQuery, kBadQueryReq, ++t);
  Frame resp0_frame = CreateFrame(0, Opcode::kError, kBadQueryErrorResp, ++t);
  Frame resp1_frame = CreateFrame(1, Opcode::kError, kBadQueryErrorResp, ++t);

  req_map[1].push_back(req1_frame);
  resp_map[0].push_back(resp0_frame);

  resp_map[1].push_back(resp1_frame);

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 1);
  EXPECT_EQ(result.records.size(), 1);
}

// To test that mis-classified frames are caught by stitcher.
TEST(CassStitcherTest, NonCQLFrames) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  int t = 0;

  Frame req0_frame = CreateFrame(0, Opcode::kQuery, {0x00, 0x00}, ++t);
  Frame resp0_frame = CreateFrame(0, Opcode::kError, {0x00, 0x00, 0x00}, ++t);
  Frame req1_frame = CreateFrame(0, Opcode::kQuery, {0x23, 0xa8, 0xf3}, ++t);
  Frame resp1_frame = CreateFrame(0, Opcode::kError, {0x35, 0x9e, 0x1b, 0x77}, ++t);

  req_map[0].push_back(req0_frame);
  req_map[0].push_back(req1_frame);
  resp_map[0].push_back(resp0_frame);
  resp_map[0].push_back(resp1_frame);

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 2);
  EXPECT_EQ(result.records.size(), 0);
}

TEST(CassStitcherTest, OpEvent) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  resp_map[1].push_back(CreateFrame(1, Opcode::kEvent, kEventResp, 3));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kRegister);
  EXPECT_EQ(record.resp.op, RespOp::kEvent);

  EXPECT_EQ(record.req.msg, "-");
  EXPECT_THAT(record.resp.msg, "SCHEMA_CHANGE DROPPED keyspace=tutorialspoint name=emp");

  // Expecting zero latency.
  EXPECT_EQ(record.req.timestamp_ns, record.resp.timestamp_ns);
}

TEST(CassStitcherTest, StartupReady) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kStartup, kStartupReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kReady, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kStartup);
  EXPECT_EQ(record.resp.op, RespOp::kReady);

  EXPECT_EQ(record.req.msg, R"({"CQL_VERSION":"3.0.0"})");
  EXPECT_THAT(record.resp.msg, IsEmpty());
}

TEST(CassStitcherTest, RegisterReady) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kRegister, kRegisterReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kReady, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kRegister);
  EXPECT_EQ(record.resp.op, RespOp::kReady);

  EXPECT_EQ(record.req.msg, R"(["TOPOLOGY_CHANGE","STATUS_CHANGE","SCHEMA_CHANGE"])");
  EXPECT_THAT(record.resp.msg, IsEmpty());
}

TEST(CassStitcherTest, OptionsSupported) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kOptions, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kSupported, kSupportedResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kOptions);
  EXPECT_EQ(record.resp.op, RespOp::kSupported);

  EXPECT_THAT(record.req.msg, IsEmpty());
  EXPECT_EQ(
      record.resp.msg,
      R"({"COMPRESSION":["snappy","lz4"],"CQL_VERSION":["3.4.4"],"PROTOCOL_VERSIONS":["3/v3","4/v4","5/v5-beta"]})");
}

TEST(CassStitcherTest, QueryResult) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kQuery, kQueryReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kResult, kResultResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kQuery);
  EXPECT_EQ(record.resp.op, RespOp::kResult);

  EXPECT_EQ(record.req.msg, "SELECT * FROM system.peers");

  EXPECT_EQ(record.resp.msg,
            "Response type = ROWS\n"
            "Number of columns = 9\n"
            R"(["peer","data_center","host_id","preferred_ip","rack",)"
            R"("release_version","rpc_address","schema_version","tokens"])"
            "\nNumber of rows = 0");
}

TEST(CassStitcherTest, QueryError) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  req_map[0].push_back(CreateFrame(0, Opcode::kQuery, kBadQueryReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kError, kBadQueryErrorResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kQuery);
  EXPECT_EQ(record.resp.op, RespOp::kError);

  EXPECT_EQ(record.req.msg, "SELECT * FROM system.schema_keyspaces ;");
  EXPECT_EQ(record.resp.msg, "[8704] unconfigured table schema_keyspaces");
}

TEST(CassStitcherTest, PrepareResult) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kPrepare, kPrepareReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kResult, kPrepareResultResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kPrepare);
  EXPECT_EQ(record.resp.op, RespOp::kResult);

  EXPECT_EQ(
      record.req.msg,
      R"(UPDATE counter1 SET "C0"="C0"+?,"C1"="C1"+?,"C2"="C2"+?,"C3"="C3"+?,"C4"="C4"+? WHERE KEY=?)");

  EXPECT_EQ(record.resp.msg, "Response type = PREPARED");
}

TEST(CassStitcherTest, ExecuteResult) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kExecute, kExecuteReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kResult, kExecuteResultResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kExecute);
  EXPECT_EQ(record.resp.op, RespOp::kResult);

  EXPECT_EQ(record.req.msg, R"(["0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("3639334E3732504E3930"])");

  EXPECT_EQ(record.resp.msg, "Response type = VOID");
}

TEST(CassStitcherTest, StartupAuthenticate) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kStartup, kStartupReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kAuthenticate, kAuthenticateResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kStartup);
  EXPECT_EQ(record.resp.op, RespOp::kAuthenticate);

  EXPECT_EQ(record.req.msg, R"({"CQL_VERSION":"3.0.0"})");
  EXPECT_THAT(record.resp.msg, "org.apache.cassandra.auth.PasswordAuthenticator");
}

TEST(CassStitcherTest, AuthResponseAuthSuccess) {
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> req_map;
  absl::flat_hash_map<stream_id_t, std::deque<Frame>> resp_map;

  RecordsWithErrorCount<Record> result;

  req_map[0].push_back(CreateFrame(0, Opcode::kAuthResponse, kAuthResponseReq, 1));
  resp_map[0].push_back(CreateFrame(0, Opcode::kAuthSuccess, kAuthSuccessResp, 2));

  result = StitchFrames(&req_map, &resp_map);
  EXPECT_TRUE(AreAllDequesEmpty(resp_map));
  EXPECT_EQ(TotalDequeSize(req_map), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.op, ReqOp::kAuthResponse);
  EXPECT_EQ(record.resp.op, RespOp::kAuthSuccess);

  EXPECT_EQ(record.req.msg, ConstStringView("\0cassandra\0cassandra"));
  EXPECT_EQ(record.resp.msg, "");
}

}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
