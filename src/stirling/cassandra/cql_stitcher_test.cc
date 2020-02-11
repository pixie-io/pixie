#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/cassandra/cql_parse.h"
#include "src/stirling/cassandra/cql_stitcher.h"

using ::testing::HasSubstr;
using ::testing::IsEmpty;

namespace pl {
namespace stirling {
namespace cass {

template <size_t N>
std::string_view StringView(const uint8_t (&a)[N]) {
  return CreateStringView<char>(CharArrayStringView<uint8_t>(a));
}

constexpr uint8_t kStartupMsg[] = {0x00, 0x01, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
                                   0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00,
                                   0x05, 0x33, 0x2e, 0x30, 0x2e, 0x30};
std::string_view kStartupMsgStr = StringView(kStartupMsg);

constexpr uint8_t kAuthResponseMsg[] = {0x00, 0x00, 0x00, 0x14, 0x00, 0x63, 0x61, 0x73,
                                        0x73, 0x61, 0x6e, 0x64, 0x72, 0x61, 0x00, 0x63,
                                        0x61, 0x73, 0x73, 0x61, 0x6e, 0x64, 0x72, 0x61};
std::string_view kAuthResponseMsgStr = StringView(kAuthResponseMsg);

constexpr uint8_t kAuthSuccessMsg[] = {0xff, 0xff, 0xff, 0xff};
std::string_view kAuthSuccessMsgStr = StringView(kAuthSuccessMsg);

constexpr uint8_t kRegisterMsg[] = {0x00, 0x03, 0x00, 0x0f, 0x54, 0x4f, 0x50, 0x4f, 0x4c, 0x4f,
                                    0x47, 0x59, 0x5f, 0x43, 0x48, 0x41, 0x4e, 0x47, 0x45, 0x00,
                                    0x0d, 0x53, 0x54, 0x41, 0x54, 0x55, 0x53, 0x5f, 0x43, 0x48,
                                    0x41, 0x4e, 0x47, 0x45, 0x00, 0x0d, 0x53, 0x43, 0x48, 0x45,
                                    0x4d, 0x41, 0x5f, 0x43, 0x48, 0x41, 0x4e, 0x47, 0x45};
std::string_view kRegisterMsgStr = StringView(kRegisterMsg);

constexpr uint8_t kQueryMsg[] = {0x00, 0x00, 0x00, 0x1a, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20,
                                 0x2a, 0x20, 0x46, 0x52, 0x4f, 0x4d, 0x20, 0x73, 0x79, 0x73, 0x74,
                                 0x65, 0x6d, 0x2e, 0x70, 0x65, 0x65, 0x72, 0x73, 0x00, 0x01, 0x00};
std::string_view kQueryMsgStr = StringView(kQueryMsg);

constexpr uint8_t kResultMsg[] = {
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
std::string_view kResultMsgStr = StringView(kResultMsg);

// Captured request packet body after issuing the following in cqlsh:
//   SELECT * FROM system.schema_keyspaces ;
constexpr uint8_t kBadQueryMsg[] = {
    0x00, 0x00, 0x00, 0x27, 0x53, 0x45, 0x4c, 0x45, 0x43, 0x54, 0x20, 0x2a, 0x20, 0x46, 0x52,
    0x4f, 0x4d, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x2e, 0x73, 0x63, 0x68, 0x65, 0x6d,
    0x61, 0x5f, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73, 0x20, 0x3b, 0x00, 0x01,
    0x34, 0x00, 0x00, 0x00, 0x64, 0x00, 0x08, 0x00, 0x05, 0x9d, 0xaf, 0x91, 0xd4, 0xc0, 0x5c};
std::string_view kBadQueryMsgStr = StringView(kBadQueryMsg);

// Captured response packet, containing:
//   unconfigured table schema_keyspaces
constexpr uint8_t kBadQueryErrorMsg[] = {
    0x00, 0x00, 0x22, 0x00, 0x00, 0x23, 0x75, 0x6e, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67,
    0x75, 0x72, 0x65, 0x64, 0x20, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x73, 0x63, 0x68,
    0x65, 0x6d, 0x61, 0x5f, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63, 0x65, 0x73};
std::string_view kBadQueryErrorMsgStr = StringView(kBadQueryErrorMsg);

constexpr uint8_t kPrepareMsg[] = {
    0x00, 0x00, 0x00, 0x5b, 0x55, 0x50, 0x44, 0x41, 0x54, 0x45, 0x20, 0x63, 0x6f, 0x75, 0x6e, 0x74,
    0x65, 0x72, 0x31, 0x20, 0x53, 0x45, 0x54, 0x20, 0x22, 0x43, 0x30, 0x22, 0x3d, 0x22, 0x43, 0x30,
    0x22, 0x2b, 0x3f, 0x2c, 0x22, 0x43, 0x31, 0x22, 0x3d, 0x22, 0x43, 0x31, 0x22, 0x2b, 0x3f, 0x2c,
    0x22, 0x43, 0x32, 0x22, 0x3d, 0x22, 0x43, 0x32, 0x22, 0x2b, 0x3f, 0x2c, 0x22, 0x43, 0x33, 0x22,
    0x3d, 0x22, 0x43, 0x33, 0x22, 0x2b, 0x3f, 0x2c, 0x22, 0x43, 0x34, 0x22, 0x3d, 0x22, 0x43, 0x34,
    0x22, 0x2b, 0x3f, 0x20, 0x57, 0x48, 0x45, 0x52, 0x45, 0x20, 0x4b, 0x45, 0x59, 0x3d, 0x3f};
std::string_view kPrepareMsgStr = StringView(kPrepareMsg);

constexpr uint8_t kPrepareResultMsg[] = {
    0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x5c, 0x6e, 0x15, 0xd4, 0x08, 0x4b, 0x0f, 0xd0, 0xd5,
    0x5a, 0x6e, 0x4b, 0x16, 0x48, 0x92, 0x7c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x05, 0x00, 0x09, 0x6b, 0x65, 0x79, 0x73, 0x70, 0x61, 0x63,
    0x65, 0x31, 0x00, 0x08, 0x63, 0x6f, 0x75, 0x6e, 0x74, 0x65, 0x72, 0x31, 0x00, 0x02, 0x43,
    0x30, 0x00, 0x05, 0x00, 0x02, 0x43, 0x31, 0x00, 0x05, 0x00, 0x02, 0x43, 0x32, 0x00, 0x05,
    0x00, 0x02, 0x43, 0x33, 0x00, 0x05, 0x00, 0x02, 0x43, 0x34, 0x00, 0x05, 0x00, 0x03, 0x6b,
    0x65, 0x79, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00};
std::string_view kPrepareResultMsgStr = StringView(kPrepareResultMsg);

constexpr uint8_t kExecuteMsg[] = {
    0x00, 0x10, 0x5c, 0x6e, 0x15, 0xd4, 0x08, 0x4b, 0x0f, 0xd0, 0xd5, 0x5a, 0x6e, 0x4b, 0x16, 0x48,
    0x92, 0x7c, 0x00, 0x0a, 0x25, 0x00, 0x06, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x36, 0x39, 0x33, 0x4e, 0x37, 0x32, 0x50, 0x4e, 0x39,
    0x30, 0x00, 0x00, 0x13, 0x88, 0x00, 0x05, 0x9e, 0x07, 0x8a, 0x50, 0xb9, 0x00};
std::string_view kExecuteMsgStr = StringView(kExecuteMsg);

constexpr uint8_t kExecuteResultMsg[] = {0x00, 0x00, 0x00, 0x01};
std::string_view kExecuteResultMsgStr = StringView(kExecuteResultMsg);

std::string_view kOptionsMsgStr = "";

constexpr uint8_t kSupportedMsg[] = {
    0x00, 0x03, 0x00, 0x11, 0x50, 0x52, 0x4f, 0x54, 0x4f, 0x43, 0x4f, 0x4c, 0x5f, 0x56, 0x45, 0x52,
    0x53, 0x49, 0x4f, 0x4e, 0x53, 0x00, 0x03, 0x00, 0x04, 0x33, 0x2f, 0x76, 0x33, 0x00, 0x04, 0x34,
    0x2f, 0x76, 0x34, 0x00, 0x09, 0x35, 0x2f, 0x76, 0x35, 0x2d, 0x62, 0x65, 0x74, 0x61, 0x00, 0x0b,
    0x43, 0x4f, 0x4d, 0x50, 0x52, 0x45, 0x53, 0x53, 0x49, 0x4f, 0x4e, 0x00, 0x02, 0x00, 0x06, 0x73,
    0x6e, 0x61, 0x70, 0x70, 0x79, 0x00, 0x03, 0x6c, 0x7a, 0x34, 0x00, 0x0b, 0x43, 0x51, 0x4c, 0x5f,
    0x56, 0x45, 0x52, 0x53, 0x49, 0x4f, 0x4e, 0x00, 0x01, 0x00, 0x05, 0x33, 0x2e, 0x34, 0x2e, 0x34};
std::string_view kSupportedMsgStr = StringView(kSupportedMsg);

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

  req_frames.push_back(CreateFrame(0, Opcode::kQuery, kBadQueryMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kError, kBadQueryErrorMsgStr, 2));

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

  Frame req0_frame = CreateFrame(0, Opcode::kQuery, kBadQueryMsgStr, ++t);
  Frame resp0_frame = CreateFrame(0, Opcode::kError, kBadQueryErrorMsgStr, ++t);
  Frame req1_frame = CreateFrame(1, Opcode::kQuery, kBadQueryMsgStr, ++t);
  Frame resp1_frame = CreateFrame(1, Opcode::kError, kBadQueryErrorMsgStr, ++t);
  Frame req2_frame = CreateFrame(2, Opcode::kQuery, kBadQueryMsgStr, ++t);
  Frame resp2_frame = CreateFrame(2, Opcode::kError, kBadQueryErrorMsgStr, ++t);

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

TEST(CassStitcherTest, OpEvent) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  resp_frames.push_back(CreateFrame(-1, Opcode::kEvent, "Foo", 3));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kRegister);
  EXPECT_EQ(record.resp.op, RespOp::kEvent);

  EXPECT_EQ(record.req.msg, "-");
  EXPECT_THAT(record.resp.msg, "Foo");

  // Expecting zero latency.
  EXPECT_EQ(record.req.timestamp_ns, record.resp.timestamp_ns);
}

TEST(CassStitcherTest, StartupReady) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kStartup, kStartupMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kReady, "", 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kStartup);
  EXPECT_EQ(record.resp.op, RespOp::kReady);

  EXPECT_EQ(record.req.msg, R"({"CQL_VERSION":"3.0.0"})");
  EXPECT_THAT(record.resp.msg, IsEmpty());
}

TEST(CassStitcherTest, RegisterReady) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kRegister, kRegisterMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kReady, "", 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kRegister);
  EXPECT_EQ(record.resp.op, RespOp::kReady);

  EXPECT_EQ(record.req.msg, R"(["TOPOLOGY_CHANGE","STATUS_CHANGE","SCHEMA_CHANGE"])");
  EXPECT_THAT(record.resp.msg, IsEmpty());
}

TEST(CassStitcherTest, OptionsSupported) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kOptions, kOptionsMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kSupported, kSupportedMsgStr, 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kOptions);
  EXPECT_EQ(record.resp.op, RespOp::kSupported);

  EXPECT_THAT(record.req.msg, IsEmpty());
  EXPECT_EQ(
      record.resp.msg,
      R"({"COMPRESSION":["snappy","lz4"],"CQL_VERSION":["3.4.4"],"PROTOCOL_VERSIONS":["3/v3","4/v4","5/v5-beta"]})");
}

TEST(CassStitcherTest, QueryResult) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kQuery, kQueryMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kResult, kResultMsgStr, 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kQuery);
  EXPECT_EQ(record.resp.op, RespOp::kResult);

  EXPECT_EQ(record.req.msg, "SELECT * FROM system.peers");

  // TODO(oazizi): Enable once parsing of results in complete.
  // EXPECT_EQ(record.resp.msg, "");
}

TEST(CassStitcherTest, PrepareResult) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kPrepare, kPrepareMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kResult, kPrepareResultMsgStr, 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kPrepare);
  EXPECT_EQ(record.resp.op, RespOp::kResult);

  EXPECT_EQ(
      record.req.msg,
      R"(UPDATE counter1 SET "C0"="C0"+?,"C1"="C1"+?,"C2"="C2"+?,"C3"="C3"+?,"C4"="C4"+? WHERE KEY=?)");

  // TODO(oazizi): Enable once parsing of results in complete.
  // EXPECT_EQ(record.resp.msg, "");
}

TEST(CassStitcherTest, ExecuteResult) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kExecute, kExecuteMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kResult, kExecuteResultMsgStr, 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kExecute);
  EXPECT_EQ(record.resp.op, RespOp::kResult);

  EXPECT_EQ(record.req.msg, R"(["0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("0000000000000001",)"
                            R"("3639334E3732504E3930"])");

  // TODO(oazizi): Enable once parsing of results in complete.
  // EXPECT_EQ(record.resp.msg, "");
}

TEST(CassStitcherTest, AuthResponseAuthSuccess) {
  std::deque<Frame> req_frames;
  std::deque<Frame> resp_frames;
  std::vector<Record> records;

  req_frames.push_back(CreateFrame(0, Opcode::kAuthResponse, kAuthResponseMsgStr, 1));
  resp_frames.push_back(CreateFrame(0, Opcode::kAuthSuccess, kAuthSuccessMsgStr, 2));

  records = ProcessFrames(&req_frames, &resp_frames);
  EXPECT_TRUE(resp_frames.empty());
  EXPECT_EQ(req_frames.size(), 0);
  ASSERT_EQ(records.size(), 1);

  Record& record = records.front();

  EXPECT_EQ(record.req.op, ReqOp::kAuthResponse);
  EXPECT_EQ(record.resp.op, RespOp::kAuthSuccess);

  EXPECT_EQ(record.req.msg, ConstStringView("\0cassandra\0cassandra"));
  EXPECT_EQ(record.resp.msg, "");
}

}  // namespace cass
}  // namespace stirling
}  // namespace pl
