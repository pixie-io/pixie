#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/dns/dns_stitcher.h"

using ::testing::HasSubstr;
using ::testing::IsEmpty;

namespace pl {
namespace stirling {
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
  f.records = {};
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
  f.records = std::move(records);
  f.timestamp_ns = timestamp_ns;

  return f;
}

//-----------------------------------------------------------------------------
// Test Cases
//-----------------------------------------------------------------------------

TEST(DnsStitcherTest, OutOfOrderMatching) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  RecordsWithErrorCount<Record> result;

  int t = 0;

  Frame req0_frame = CreateReqFrame(++t, 0);
  Frame resp0_frame = CreateRespFrame(++t, 0, std::vector<DNSRecord>());
  Frame req1_frame = CreateReqFrame(++t, 1);
  Frame resp1_frame = CreateRespFrame(++t, 1, std::vector<DNSRecord>());
  Frame req2_frame = CreateReqFrame(++t, 2);
  Frame resp2_frame = CreateRespFrame(++t, 2, std::vector<DNSRecord>());

  result = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  req_frames.push_back(req0_frame);
  req_frames.push_back(req1_frame);

  result = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);

  resp_frames.push_back(resp1_frame);

  result = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 2);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);

  req_frames.push_back(req2_frame);
  resp_frames.push_back(resp0_frame);

  result = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 1);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);

  resp_frames.push_back(resp2_frame);

  result = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(resp_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 1);

  result = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(resp_frames.size(), 0);
  EXPECT_EQ(result.error_count, 0);
  EXPECT_EQ(result.records.size(), 0);
}

}  // namespace dns
}  // namespace stirling
}  // namespace pl
