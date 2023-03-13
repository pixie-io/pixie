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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/inet_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/dns/stitcher.h"
#include "src/stirling/testing/common.h"

using ::testing::HasSubstr;
using ::testing::IsEmpty;

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

//-----------------------------------------------------------------------------
// Test Utils
//-----------------------------------------------------------------------------

DNSHeader header;
std::vector<DNSRecord> records;

Frame CreateReqFrame(uint64_t timestamp_ns, uint16_t txid) {
  Frame f;
  f.header.txid = txid;
  f.header.flags = 0x0100;  // Standard query.
  f.header.num_queries = 1;
  f.header.num_answers = 0;
  f.header.num_auth = 0;
  f.header.num_addl = 0;
  f.timestamp_ns = timestamp_ns;
  return f;
}

Frame CreateRespFrame(uint64_t timestamp_ns, uint16_t txid, std::vector<DNSRecord> records) {
  Frame f;
  f.header.txid = txid;
  f.header.flags = 0x8180;  // Standard query response, No error
  f.header.num_queries = 1;
  f.header.num_answers = records.size();
  f.header.num_auth = 0;
  f.header.num_addl = 0;
  f.AddRecords(std::move(records));
  f.timestamp_ns = timestamp_ns;

  return f;
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

TEST(DnsStitcherTest, RecordOutput) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  RecordsWithErrorCount<Record> result;

  InetAddr ip_addr;
  ip_addr.family = InetAddrFamily::kIPv4;
  struct in_addr addr_tmp;
  PX_CHECK_OK(ParseIPv4Addr("1.2.3.4", &addr_tmp));
  ip_addr.addr = addr_tmp;

  std::vector<DNSRecord> dns_records;
  dns_records.push_back(DNSRecord{"pixie.ai", "", ip_addr});

  int t = 0;
  Frame req0_frame = CreateReqFrame(++t, 0);
  Frame resp0_frame = CreateRespFrame(++t, 0, dns_records);

  req_frames.push_back(req0_frame);
  resp_frames.push_back(resp0_frame);

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  ASSERT_EQ(result.records.size(), 1);

  Record& record = result.records.front();

  EXPECT_EQ(record.req.timestamp_ns, 1);
  EXPECT_EQ(record.req.header,
            R"({"txid":0,"qr":0,"opcode":0,"aa":0,"tc":0,"rd":1,"ra":0,"ad":0,"cd":0,"rcode":0,)"
            R"("num_queries":1,"num_answers":0,"num_auth":0,"num_addl":0})");
  EXPECT_EQ(record.req.query, R"({"queries":[]})");

  EXPECT_EQ(record.resp.timestamp_ns, 2);
  EXPECT_EQ(record.resp.header,
            R"({"txid":0,"qr":1,"opcode":0,"aa":0,"tc":0,"rd":1,"ra":1,"ad":0,"cd":0,"rcode":0,)"
            R"("num_queries":1,"num_answers":1,"num_auth":0,"num_addl":0})");
  EXPECT_EQ(record.resp.msg, R"({"answers":[{"name":"pixie.ai","type":"A","addr":"1.2.3.4"}]})");
}

TEST(DnsStitcherTest, OutOfOrderMatching) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  RecordsWithErrorCount<Record> result;

  PX_SET_FOR_SCOPE(FLAGS_include_respless_dns_requests, true);

  int t = 0;

  Frame req0_frame = CreateReqFrame(++t, 0);
  Frame resp0_frame = CreateRespFrame(++t, 0, std::vector<DNSRecord>());
  Frame req1_frame = CreateReqFrame(++t, 1);
  Frame resp1_frame = CreateRespFrame(++t, 1, std::vector<DNSRecord>());
  Frame req2_frame = CreateReqFrame(++t, 2);
  Frame resp2_frame = CreateRespFrame(++t, 2, std::vector<DNSRecord>());

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  req_frames.push_back(req0_frame);
  req_frames.push_back(req1_frame);

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), FLAGS_include_respless_dns_requests ? 2 : 0);

  resp_frames.push_back(resp1_frame);

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), FLAGS_include_respless_dns_requests ? 2 : 1);

  req_frames.push_back(req2_frame);
  resp_frames.push_back(resp0_frame);

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 1);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), FLAGS_include_respless_dns_requests ? 2 : 1);

  resp_frames.push_back(resp2_frame);

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(resp_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);

  result = StitchFrames(&req_frames, &resp_frames, FLAGS_include_respless_dns_requests);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(resp_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
