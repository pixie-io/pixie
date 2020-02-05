#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/cassandra/cql_parse.h"
#include "src/stirling/cassandra/cql_stitcher.h"

using ::testing::HasSubstr;

namespace pl {
namespace stirling {
namespace cass {

// Captured request packet body after issuing the following in cqlsh:
//   SELECT * FROM system.schema_keyspaces ;
constexpr uint8_t kQueryMsg[] = {
    0x00, 0x00, 0x00, 0x27, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x2a, 0x20, 0x46, 0x52,
    0x4f, 0x4d, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e, 0x73, 0x63, 0x68, 0x65, 0x6d,
    0x61, 0x5f, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73, 0x20, 0x3b, 0x00, 0x01,
    0x34, 0x00, 0x00, 0x00, 0x64, 0x00, 0x08, 0x00, 0x05, 0x9d, 0xaf, 0x91, 0xd4, 0xc0, 0x5c};
std::string_view kQueryMsgStr = CreateStringView<char>(CharArrayStringView<uint8_t>(kQueryMsg));

// Captured response packet, containing:
//   unconfigured table schema_keyspaces
constexpr uint8_t kErrorMsg[] = {0x00, 0x00, 0x22, 0x00, 0x00, 0x23, 0x75, 0x6e, 0x63, 0x6f, 0x6e,
                                 0x66, 0x69, 0x67, 0x75, 0x72, 0x65, 0x64, 0x20, 0x74, 0x61, 0x62,
                                 0x6c, 0x65, 0x20, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0x5f, 0x6b,
                                 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73};
std::string_view kErrorMsgStr = CreateStringView<char>(CharArrayStringView<uint8_t>(kErrorMsg));

Frame CreateFrame(uint16_t stream, Opcode opcode, std::string_view msg, uint64_t timestamp_ns) {
  Frame f;
  f.hdr.opcode = opcode;
  f.hdr.stream = stream;
  f.hdr.flags = 0;
  f.hdr.flags = 0x04;
  f.hdr.length = msg.length();
  f.msg = msg;
  f.timestamp_ns = timestamp_ns;
  return f;
}

TEST(CassStitcherTest, Basic) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  EXPECT_EQ(records.size(), 0);

  req_frames.push_back(CreateFrame(0, Opcode::kQuery, kQueryMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kError, kErrorMsgStr, 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kQuery);
  EXPECT_EQ(record.resp.op, RespOp::kError);

  EXPECT_THAT(record.req.msg, HasSubstr("SELECT * FROM system.schema_keyspaces ;"));
  EXPECT_THAT(record.resp.msg, HasSubstr("unconfigured table schema_keyspaces"));
}

TEST(CassStitcherTest, OutOfOrder) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  int t = 0;

  Frame req0_frame = CreateFrame(0, Opcode::kQuery, kQueryMsgStr, ++t);
  Frame resp0_frame = CreateFrame(0, Opcode::kError, kErrorMsgStr, ++t);
  Frame req1_frame = CreateFrame(1, Opcode::kQuery, kQueryMsgStr, ++t);
  Frame resp1_frame = CreateFrame(1, Opcode::kError, kErrorMsgStr, ++t);
  Frame req2_frame = CreateFrame(2, Opcode::kQuery, kQueryMsgStr, ++t);
  Frame resp2_frame = CreateFrame(2, Opcode::kError, kErrorMsgStr, ++t);

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  EXPECT_EQ(records.size(), 0);

  req_frames.push_back(req0_frame);
  req_frames.push_back(req1_frame);

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 2);
  EXPECT_EQ(records.size(), 0);

  resp_frames.push_back(resp1_frame);

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 2);
  EXPECT_EQ(records.size(), 1);

  req_frames.push_back(req2_frame);
  resp_frames.push_back(resp0_frame);

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 1);
  EXPECT_EQ(records.size(), 1);

  resp_frames.push_back(resp2_frame);

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(resp_frames.size(), 0);
  EXPECT_EQ(records.size(), 1);

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(resp_frames.size(), 0);
  EXPECT_EQ(records.size(), 0);
}

}  // namespace cass
}  // namespace stirling
}  // namespace pl
