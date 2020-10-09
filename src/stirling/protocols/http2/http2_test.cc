#include "src/stirling/protocols/http2/http2.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
extern "C" {
#include <nghttp2/nghttp2_frame.h>
#include <nghttp2/nghttp2_helper.h>
}

#include "src/common/base/error.h"
#include "src/common/base/status.h"
#include "src/common/testing/testing.h"
#include "src/stirling/protocols/http2/testing/utils.h"
#include "src/stirling/testing/event_generator.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace http2 {

using ::pl::grpc::MethodInputOutput;
using ::pl::grpc::ServiceDescriptorDatabase;
using ::pl::stirling::protocols::http2::testing::GreetServiceFDSet;
using ::pl::stirling::testing::DataEventWithTimestamp;
using ::pl::testing::proto::EqualsProto;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(UnpackFrameTest, TestVariousCases) {
  struct TestCase {
    std::string desc;
    std::string input;
    std::string expected_result_buf;
    ParseState expected_parse_state;
  };

  const std::vector<TestCase> test_cases = {
      {"Incomplete frame header should return kNeedsMoreData", std::string(8, ' '),
       std::string(8, ' '), ParseState::kNeedsMoreData},
      {"Valid HEADERS frame",
       std::string{"\x0\x0\x3\x1\x4\x0\x0\x0\x1"
                   "a:b",
                   12},
       "", ParseState::kSuccess},
      {"Incomplete frame body should return kNeedsMoreData",
       std::string{"\x0\x0\x2\x1\x1\x0\x0\x0\x1", 9},
       {"\x0\x0\x2\x1\x1\x0\x0\x0\x1", 9},
       ParseState::kNeedsMoreData},
      {"Zero-sized HEADERS frame", std::string{"\x0\x0\x0\x1\x1\x0\x0\x0\x1", 9}, "",
       ParseState::kSuccess},
      {"SETTINGS frame is ignored", std::string{"\x0\x0\x0\x4\x1\x0\x0\x0\x1", 9}, "",
       ParseState::kIgnored},
      {"DATA frame with padding but frame length is 0",
       std::string{"\x0\x0\x0\x0\x8\x0\x0\x0\x1", 9}, "", ParseState::kInvalid},
      {"DATA frame body smaller than padding length",
       std::string{"\x0\x0\x1\x0\x8\x0\x0\x0\x1\x3", 10}, "", ParseState::kInvalid},
      {"HEADERS frame with padding but frame length is 0",
       std::string{"\x0\x0\x0\x1\x8\x0\x0\x0\x1", 9}, "", ParseState::kInvalid},
      {"HEADERS frame body smaller than padding length",
       std::string{"\x0\x0\x1\x1\x8\x0\x0\x0\x1\x3", 10}, "", ParseState::kInvalid},
  };

  for (const TestCase& c : test_cases) {
    std::string_view buf = c.input;
    Frame frame;
    ParseState s = UnpackFrame(&buf, &frame);
    EXPECT_THAT(s, Eq(c.expected_parse_state)) << c.desc;
    EXPECT_THAT(std::string(buf), StrEq(c.expected_result_buf)) << c.desc;
  }
}

std::map<std::string, std::string> Headers(const Frame& frame) {
  std::map<std::string, std::string> result;
  for (size_t i = 0; i < frame.frame.headers.nvlen; ++i) {
    std::string name(reinterpret_cast<const char*>(frame.frame.headers.nva[i].name),
                     frame.frame.headers.nva[i].namelen);
    std::string value(reinterpret_cast<const char*>(frame.frame.headers.nva[i].value),
                      frame.frame.headers.nva[i].valuelen);
    result[name] = value;
  }
  return result;
}

u8string_view ToU8(std::string_view buf) {
  return u8string_view(reinterpret_cast<const uint8_t*>(buf.data()), buf.size());
}

std::string_view ToChar(u8string_view buf) {
  return std::string_view(reinterpret_cast<const char*>(buf.data()), buf.size());
}

void PrintTo(const Frame& frame, std::ostream* os) {
  *os << "frame type: " << static_cast<int>(frame.frame.hd.type);
  *os << " payload: " << BytesToString<bytes_format::Hex>(ToChar(frame.u8payload));
}

MATCHER_P2(MatchesTypePayload, t, p,
           absl::Substitute("Matches type $0 with payload $1", ::testing::PrintToString(t),
                            ::testing::PrintToString(p))) {
  return arg.frame.hd.type == t && arg.u8payload == ToU8(p);
}

class HTTP2ParserTest : public ::testing::Test {
 protected:
  EventParser parser_;
};

TEST_F(HTTP2ParserTest, UnpackFrames) {
  constexpr std::string_view kMagic = ConstStringView(NGHTTP2_CLIENT_MAGIC);
  constexpr std::string_view kHdrFrame = ConstStringView(
      "\x0\x0\x3\x1\x4\x0\x0\x0\x1"
      "a:b");
  constexpr std::string_view kContFrame = ConstStringView(
      "\x0\x0\x3\x9\x4\x0\x0\x0\x1"
      "c:d");
  constexpr std::string_view kDataFrame = ConstStringView(
      "\x0\x0\x4\x0\x1\x0\x0\x0\x2"
      "abcd");
  constexpr std::string_view kIncompleteFrame = ConstStringView("\x0\x0\x1\x1\x1\x0\x0\x0\x3");

  const std::string kBuf =
      absl::StrCat(kMagic, kHdrFrame, kContFrame, kDataFrame, kIncompleteFrame);

  const size_t kFrame1Pos = kMagic.size();
  const size_t kFrame2Pos = kFrame1Pos + kHdrFrame.size();
  const size_t kFrame3Pos = kFrame2Pos + kContFrame.size();
  const size_t kFrame4Pos = kFrame3Pos + kDataFrame.size();

  std::deque<Frame> frames;
  ParseResult<size_t> res = parser_.ParseFramesLoop(MessageType::kUnknown, kBuf, &frames);
  EXPECT_THAT(res.state, Eq(ParseState::kNeedsMoreData));
  EXPECT_THAT(res.frame_positions, ElementsAre(StartEndPos<size_t>{kFrame1Pos, kFrame2Pos - 1},
                                               StartEndPos<size_t>{kFrame2Pos, kFrame3Pos - 1},
                                               StartEndPos<size_t>{kFrame3Pos, kFrame4Pos - 1}));
  EXPECT_THAT(res.end_position, Eq(kFrame4Pos))
      << "End position does not go into the incomplete frame";
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c:d"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abcd")));
}

// Returns a vector of single HTTP2Message constructed from the input.
HTTP2Message GRPCMsg(MessageType type, std::string_view msg, NVMap headers) {
  HTTP2Message res;
  res.type = type;
  res.message = msg;
  res.headers = std::move(headers);
  return res;
}

MATCHER_P2(HasMsgAndHdrs, msg, hdrs, "") { return arg.message == msg && arg.headers == hdrs; }

TEST(MatchGRPCReqRespTest, InputsAreMoved) {
  std::map<uint32_t, HTTP2Message> reqs{{1u, GRPCMsg(MessageType::kRequest, "a", {{"h1", "v1"}})},
                                        {2u, GRPCMsg(MessageType::kRequest, "b", {{"h2", "v2"}})}};
  std::map<uint32_t, HTTP2Message> resps{
      {0u, GRPCMsg(MessageType::kResponse, "c", {{"h3", "v3"}})},
      {1u, GRPCMsg(MessageType::kResponse, "d", {{"h4", "v4"}})}};

  std::vector<Record> matched_msgs = MatchGRPCReqResp(std::move(reqs), std::move(resps));
  ASSERT_THAT(matched_msgs, SizeIs(1));
  const HTTP2Message& req = matched_msgs.begin()->req;
  const HTTP2Message& resp = matched_msgs.begin()->resp;
  EXPECT_EQ("a", req.message);
  EXPECT_EQ("d", resp.message);
}

std::string PackFrame(nghttp2_frame_type type, std::string_view msg, uint8_t flags,
                      uint32_t stream_id) {
  std::string res(NGHTTP2_FRAME_HDLEN + msg.size(), '\0');
  uint8_t* buf = reinterpret_cast<uint8_t*>(res.data());
  nghttp2_frame_hd hd = {};
  hd.length = msg.size();
  hd.type = type;
  hd.flags = flags;
  hd.stream_id = stream_id;
  nghttp2_frame_pack_frame_hd(buf, &hd);
  res.replace(NGHTTP2_FRAME_HDLEN, msg.size(), msg);
  return res;
}

std::string PackHeadersFrame(std::string_view msg, uint8_t flags, uint32_t stream_id) {
  return PackFrame(NGHTTP2_HEADERS, msg, flags, stream_id);
}

std::string PackDataFrame(std::string_view msg, uint8_t flags, uint32_t stream_id) {
  return PackFrame(NGHTTP2_DATA, msg, flags, stream_id);
}

std::string PackContinuationFrame(std::string_view msg, uint8_t flags, uint32_t stream_id) {
  return PackFrame(NGHTTP2_CONTINUATION, msg, flags, stream_id);
}

std::vector<const Frame*> ExtractPtrs(const std::deque<Frame>& frames) {
  std::vector<const Frame*> frame_ptrs;
  for (const Frame& f : frames) {
    frame_ptrs.push_back(&f);
  }
  return frame_ptrs;
}

TEST_F(HTTP2ParserTest, SuccessiveHeadersFrameCausesError) {
  std::string input = absl::StrCat(PackHeadersFrame("", 0, 1), PackHeadersFrame("", 0, 1));
  std::deque<Frame> frames;
  ParseResult<size_t> res = parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);
  EXPECT_THAT(frames, SizeIs(2));

  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);

  std::vector<HTTP2Message> msgs;
  EXPECT_EQ(ParseState::kInvalid, StitchGRPCMessageFrames(ExtractPtrs(frames), &msgs));
}

TEST_F(HTTP2ParserTest, DataAfterHeadersCausesError) {
  std::string input = absl::StrCat(PackHeadersFrame("", 0, 1), PackDataFrame("abcd", 0, 1));
  std::deque<Frame> frames;
  ParseResult<size_t> res = parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);
  EXPECT_THAT(frames, SizeIs(2));

  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);

  std::vector<HTTP2Message> msgs;
  EXPECT_EQ(ParseState::kInvalid, StitchGRPCMessageFrames(ExtractPtrs(frames), &msgs));
}

TEST_F(HTTP2ParserTest, StitchReqsRespsOfDifferentStreams) {
  std::string input =
      absl::StrCat(PackHeadersFrame("", NGHTTP2_FLAG_END_HEADERS, 1),
                   PackHeadersFrame("", NGHTTP2_FLAG_END_HEADERS, 2), PackDataFrame("abcd", 0, 1),
                   PackDataFrame("abcd", NGHTTP2_FLAG_END_STREAM, 2),
                   PackHeadersFrame("", NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_END_STREAM, 1));
  std::map<uint32_t, HTTP2Message> stream_msgs;
  std::deque<Frame> frames;
  ParseResult<size_t> res = parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);
  ASSERT_THAT(frames, SizeIs(5));

  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);

  StitchFramesToGRPCMessages(frames, &stream_msgs);
  // There should be one gRPC request and response.
  ASSERT_THAT(stream_msgs, ElementsAre(Pair(1, _), Pair(2, _)));

  const HTTP2Message& req_msg = stream_msgs[2];
  EXPECT_EQ(MessageType::kRequest, req_msg.type);
  EXPECT_EQ("abcd", req_msg.message);
  EXPECT_THAT(req_msg.frames, ElementsAre(&frames[1], &frames[3]));

  const HTTP2Message& resp_msg = stream_msgs[1];
  EXPECT_EQ(MessageType::kResponse, resp_msg.type);
  EXPECT_EQ("abcd", resp_msg.message);
  // Note we put the HEADERS frames first, and then DATA frames.
  EXPECT_THAT(resp_msg.frames, ElementsAre(&frames[0], &frames[4], &frames[2]));
}

TEST_F(HTTP2ParserTest, IncompleteMessage) {
  std::string input =
      absl::StrCat(PackHeadersFrame("", NGHTTP2_FLAG_END_HEADERS, 1), PackDataFrame("abcd", 0, 2));
  std::deque<Frame> frames;
  ParseResult<size_t> res = parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);

  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);

  std::map<uint32_t, HTTP2Message> stream_msgs;
  StitchFramesToGRPCMessages(frames, &stream_msgs);
  EXPECT_THAT(stream_msgs, IsEmpty()) << "There is no END_STREAM in frames, so there is no data";
}

struct NV {
  std::string name;
  std::string value;
};

// Use std::vector<NV> instead of std::map<NV> to preserve the order of the input.
u8string Deflate(nghttp2_hd_deflater* deflater, const std::vector<NV>& nv_list) {
  std::vector<nghttp2_nv> nvs;
  nvs.reserve(nv_list.size());
  for (const auto& nv : nv_list) {
    nvs.push_back(nghttp2_nv{reinterpret_cast<uint8_t*>(const_cast<char*>(nv.name.data())),
                             reinterpret_cast<uint8_t*>(const_cast<char*>(nv.value.data())),
                             nv.name.size(), nv.value.size(), NGHTTP2_NV_FLAG_NONE});
  }
  size_t buflen = nghttp2_hd_deflate_bound(deflater, nvs.data(), nvs.size());
  u8string buf(buflen, '\0');

  ssize_t rv = nghttp2_hd_deflate_hd(deflater, buf.data(), buf.size(), nvs.data(), nvs.size());
  buf.resize(rv < 0 ? 0 : rv);
  return buf;
}

TEST(DeflateEnflateTest, DeflateInflateOutOfOrder) {
  nghttp2_hd_deflater* deflater;
  nghttp2_hd_inflater* inflater;

  ASSERT_EQ(0, nghttp2_hd_deflate_new(&deflater, 4096));
  ASSERT_EQ(0, nghttp2_hd_inflate_new(&inflater));

  u8string deflate_nva1 = Deflate(deflater, {{"h1", "v1"}});
  u8string deflate_nva2 = Deflate(deflater, {{"h2", "v2"}});
  u8string deflate_nva3 = Deflate(deflater, {{"h1", "v1"}});
  u8string deflate_nva4 = Deflate(deflater, {{"h2", "v2"}});

  NVMap nv_map1, nv_map2, nv_map3, nv_map4;
  // Note deflate_nva1 and deflate_nva2 are inflated in reverse order.
  EXPECT_EQ(ParseState::kSuccess, InflateHeaderBlock(inflater, deflate_nva2, &nv_map2));
  EXPECT_EQ(ParseState::kSuccess, InflateHeaderBlock(inflater, deflate_nva1, &nv_map1));
  EXPECT_EQ(ParseState::kSuccess, InflateHeaderBlock(inflater, deflate_nva3, &nv_map3));
  EXPECT_EQ(ParseState::kSuccess, InflateHeaderBlock(inflater, deflate_nva4, &nv_map4));

  EXPECT_THAT(nv_map1, ElementsAre(Pair("h1", "v1")));
  EXPECT_THAT(nv_map2, ElementsAre(Pair("h2", "v2")));
  // The encoder would encoded h2 with the same value as the first name-value pair, i.e.,
  // "h1":"v1". But when decoding, we first decode "h2":"v2", which causes it takes the code words
  // for "h1":"v1". So nv_map3 becomes "h2":"v2", instead of "h1":"v1".
  EXPECT_THAT(nv_map3, ElementsAre(Pair("h2", "v2")));
  EXPECT_THAT(nv_map4, ElementsAre(Pair("h1", "v1")));

  nghttp2_hd_inflate_del(inflater);
  nghttp2_hd_deflate_del(deflater);
}

std::ostream& operator<<(std::ostream& os, u8string_view buf) {
  os << std::string_view(reinterpret_cast<const char*>(buf.data()), buf.size());
  return os;
}

#define BUILD_FIELD(var, field) var.field
#define COMPARE(lhs, rhs, field)                                                               \
  if (BUILD_FIELD(lhs, field) != BUILD_FIELD(rhs, field)) {                                    \
    LOG(INFO) << #field ": " << BUILD_FIELD(lhs, field) << " vs. " << BUILD_FIELD(rhs, field); \
    return false;                                                                              \
  }

template <typename LiteralFieldType>
bool Comp(const LiteralFieldType& lhs, const LiteralFieldType& rhs) {
  COMPARE(lhs, rhs, update_dynamic_table);
  if (lhs.name != rhs.name) {
    if (lhs.name.index() == lhs.name.index()) {
      if (std::holds_alternative<uint32_t>(lhs.name)) {
        LOG(INFO) << "name index: " << std::get<uint32_t>(lhs.name) << " vs. "
                  << std::get<uint32_t>(rhs.name);
      }
      if (std::holds_alternative<u8string_view>(lhs.name)) {
        LOG(INFO) << "name index: " << std::get<u8string_view>(lhs.name) << " vs. "
                  << std::get<u8string_view>(rhs.name);
      }
    } else {
      LOG(INFO) << "name.index(): " << lhs.name.index() << " vs. " << rhs.name.index();
    }
    return false;
  }
  COMPARE(lhs, rhs, is_name_huff_encoded);
  COMPARE(lhs, rhs, is_value_huff_encoded);
  COMPARE(lhs, rhs, value);
  return true;
}

bool operator==(const LiteralHeaderField& lhs, const LiteralHeaderField& rhs) {
  return Comp(lhs, rhs);
}
bool operator==(const IndexedHeaderField& lhs, const IndexedHeaderField& rhs) {
  COMPARE(lhs, rhs, index);
  return true;
}
bool operator==(const TableSizeUpdate& lhs, const TableSizeUpdate& rhs) {
  COMPARE(lhs, rhs, size);
  return true;
}

template <size_t N>
u8string_view U8StringView(const char (&arr)[N]) {
  return u8string_view(reinterpret_cast<const uint8_t*>(arr), N - 1);
}

TEST(DeflateEnflateTest, RandomGeneratedHeaderFields) {
  nghttp2_hd_deflater* deflater;

  // 128 is less than the default, which will result into a table size update field in the encoded
  // header block.
  ASSERT_EQ(0, nghttp2_hd_deflate_new(&deflater, /*table_size*/ 128));

  // Use very simple strings to not trigger huffman encoding. Nghttp2 automatically use huffman
  // encoding if the encoded bytes is less than the original texts; and there is no interface to
  // disable that, which makes testing difficult.
  std::vector<NV> nvs = {{"n1", "v1"}, {"n2", "v2"}, {"n3", "v3"},
                         {"n1", "v1"}, {"n2", "v2"}, {"n3", "v3"}};

  u8string header_block = Deflate(deflater, nvs);
  u8string_view buf = header_block;
  std::vector<HeaderField> fields;
  EXPECT_EQ(ParseState::kSuccess, ParseHeaderBlock(&buf, &fields));

  HeaderField field0(TableSizeUpdate{128});
  HeaderField field1(
      LiteralHeaderField{true, false, U8StringView("n1"), false, U8StringView("v1")});
  HeaderField field2(
      LiteralHeaderField{true, false, U8StringView("n2"), false, U8StringView("v2")});
  HeaderField field3(
      LiteralHeaderField{true, false, U8StringView("n3"), false, U8StringView("v3")});
  // The indexed entry takes higher index if appears earlier. So n1:v1 would start with 62, but
  // eventually bumped to 64 after 2 new indexed entires.
  HeaderField field4(IndexedHeaderField{64});
  HeaderField field5(IndexedHeaderField{63});
  HeaderField field6(IndexedHeaderField{62});
  EXPECT_THAT(fields, ElementsAre(field0, field1, field2, field3, field4, field5, field6));

  nghttp2_hd_deflate_del(deflater);
}

TEST(HeadersTest, GetHeaderValue) {
  HTTP2Message req;
  req.headers.emplace(":path", "/pl.stirling.protocols.http2.testing.Greeter/SayHello");
  req.headers.emplace("foo", "200");

  EXPECT_EQ(req.headers.ValueByKey(":path"),
            "/pl.stirling.protocols.http2.testing.Greeter/SayHello");
  EXPECT_EQ(req.headers.ValueByKey("foo"), "200");
  EXPECT_EQ(req.headers.ValueByKey("foo", "-1"), "200");
  EXPECT_EQ(req.headers.ValueByKey("missing"), "");
  EXPECT_EQ(req.headers.ValueByKey("missing", "-1"), "-1");
}

struct BoundaryTestCase {
  std::string_view input;
  MessageType msg_type;
  size_t exp_res;
};

using FindFrameBoundaryTest = ::testing::TestWithParam<BoundaryTestCase>;

TEST_P(FindFrameBoundaryTest, CheckReturnValues) {
  auto param = GetParam();
  EXPECT_EQ(param.exp_res, FindFrameBoundary<Frame>(param.msg_type, param.input, 0));
}

INSTANTIATE_TEST_SUITE_P(
    FindFrameBoundarySuite, FindFrameBoundaryTest,
    ::testing::Values(
        BoundaryTestCase{
            ConstStringView("abcd\x00\x00\x04\x01\x04\x00\x00\x00\x0D\x86\x83\xC0\xBF"),
            MessageType::kRequest, 4},
        BoundaryTestCase{ConstStringView("abcd\x00\x00\x04\x01\x04\x00\x00\x00\x0D\x86\x83"),
                         MessageType::kRequest, std::string_view::npos},
        BoundaryTestCase{ConstStringView("abcd"), MessageType::kRequest, std::string_view::npos},
        BoundaryTestCase{ConstStringView("abcd\x00\x00\x04\x01\x04\x00\x00\x00\x0D\x88"),
                         MessageType::kResponse, 4},
        BoundaryTestCase{ConstStringView("abcd\x00\x00\x04\x01\x04\x00\x00\x00\x0D"),
                         MessageType::kResponse, std::string_view::npos},
        BoundaryTestCase{ConstStringView("abcd"), MessageType::kResponse, std::string_view::npos},
        BoundaryTestCase{std::string_view(), MessageType::kRequest, std::string_view::npos},
        BoundaryTestCase{std::string_view(), MessageType::kResponse, std::string_view::npos}));

TEST(DecodeIntegerTest, AllCases) {
  {
    // TODO(yzhao): Consider change StringView() for uint8_t.
    constexpr uint8_t kBuf[] = "\xC5";
    u8string_view buf(kBuf, sizeof(kBuf) - 1);
    uint32_t res = 0;
    EXPECT_EQ(ParseState::kSuccess, DecodeInteger(&buf, /*prefix*/ 7, &res));
    EXPECT_EQ(69, res);
    EXPECT_THAT(buf, IsEmpty());
  }
  {
    // \xFF indicates there is trailing bytes. \x01 terminates the sequence. The final value is
    // 1+127==128.
    constexpr uint8_t kBuf[] = "\xFF\x01";
    u8string_view buf(kBuf, sizeof(kBuf) - 1);
    uint32_t res = 0;
    EXPECT_EQ(ParseState::kSuccess, DecodeInteger(&buf, /*prefix*/ 7, &res));
    EXPECT_EQ(128, res);
    EXPECT_THAT(buf, IsEmpty());
  }
  {
    // \xFF indicates there is trailing bytes. Because there is no additional bytes, kNeedsMoreData
    // is returned.
    constexpr uint8_t kBuf[] = "\xFF";
    u8string_view buf(kBuf, sizeof(kBuf) - 1);
    uint32_t res = 0;
    EXPECT_EQ(ParseState::kNeedsMoreData, DecodeInteger(&buf, /*prefix*/ 7, &res));
    EXPECT_EQ(u8string_view(kBuf, sizeof(kBuf) - 1), buf);
  }
}

TEST_F(HTTP2ParserTest, HeadersAndContinuationFramesAreStitched) {
  // \x86 & \x83 are static code can always be decode correctly.
  const std::string input = absl::StrCat(
      PackHeadersFrame("\x86", 0, 1), PackContinuationFrame("\x83", NGHTTP2_FLAG_END_HEADERS, 1));
  std::deque<Frame> frames;
  parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "\x86"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "\x83")));
  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "\x86\x83"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "\x83")));
  ASSERT_THAT(frames, SizeIs(2));

  EXPECT_FALSE(frames[0].consumed);
  EXPECT_EQ(ParseState::kSuccess, frames[0].headers_parse_state);
  EXPECT_THAT(frames[0].headers, ElementsAre(Pair(":method", "POST"), Pair(":scheme", "http")));

  EXPECT_TRUE(frames[1].consumed);
  EXPECT_EQ(ParseState::kUnknown, frames[1].headers_parse_state);
}

TEST_F(HTTP2ParserTest, HeadersAndThenDataFrame) {
  const std::string input =
      absl::StrCat(PackHeadersFrame("a:b", 0, 1), PackContinuationFrame("c:d", 0, 1),
                   PackDataFrame("abc", 1, 2));
  std::deque<Frame> frames;
  parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c:d"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abc")));
  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:bc:d"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c:d"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abc")));
  ASSERT_THAT(frames, SizeIs(3));

  EXPECT_FALSE(frames[0].consumed);
  EXPECT_EQ(ParseState::kInvalid, frames[0].headers_parse_state);

  EXPECT_TRUE(frames[1].consumed);
  EXPECT_EQ(ParseState::kUnknown, frames[1].headers_parse_state);
}

TEST_F(HTTP2ParserTest, SuccessiveHeadersFrames) {
  const std::string input =
      absl::StrCat(PackHeadersFrame("a:b", 0, 1), PackHeadersFrame("c:d", 0, 1));
  std::deque<Frame> frames;
  parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_HEADERS, "c:d")));
  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a:b"),
                                  MatchesTypePayload(NGHTTP2_HEADERS, "c:d")));
  ASSERT_THAT(frames, SizeIs(2));

  EXPECT_FALSE(frames[0].consumed);
  EXPECT_EQ(ParseState::kInvalid, frames[0].headers_parse_state);

  EXPECT_FALSE(frames[1].consumed);
  EXPECT_EQ(ParseState::kUnknown, frames[1].headers_parse_state);
}

TEST_F(HTTP2ParserTest, SuccessiveContinuationFrames) {
  std::string input =
      absl::StrCat(PackContinuationFrame("a", 0, 1), PackContinuationFrame("b", 0, 1),
                   PackContinuationFrame("c", 9, 1));
  std::deque<Frame> frames;
  parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_CONTINUATION, "a"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c")));
  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "abc"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "b"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "c")));
  ASSERT_THAT(frames, SizeIs(3));

  EXPECT_FALSE(frames[0].consumed);
  EXPECT_EQ(ParseState::kInvalid, frames[0].headers_parse_state);

  EXPECT_TRUE(frames[1].consumed);
  EXPECT_EQ(ParseState::kUnknown, frames[1].headers_parse_state);

  EXPECT_TRUE(frames[2].consumed);
  EXPECT_EQ(ParseState::kUnknown, frames[2].headers_parse_state);
}

// Tests that the first HEADERS frame of an incomplete header block is left intact.
TEST_F(HTTP2ParserTest, IncompleteHeaderBlock) {
  std::string input = absl::StrCat(PackHeadersFrame("a", 0, 1), PackContinuationFrame("b", 0, 1));
  std::deque<Frame> frames;
  parser_.ParseFramesLoop(MessageType::kUnknown, input, &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "a"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "b")));

  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "ab"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "b")));
  ASSERT_THAT(frames, SizeIs(2));

  EXPECT_FALSE(frames[0].consumed);
  // Should still indicate that this header block is unprocessed.
  EXPECT_EQ(ParseState::kUnknown, frames[0].headers_parse_state);
  EXPECT_EQ(ParseState::kUnknown, frames[0].frame_sync_state);

  EXPECT_TRUE(frames[1].consumed);

  std::map<uint32_t, HTTP2Message> stream_msgs;
  EXPECT_EQ(ParseState::kSuccess, StitchFramesToGRPCMessages(frames, &stream_msgs));
  EXPECT_THAT(stream_msgs, IsEmpty());
}

// TODO(yzhao): It's quite complicated to stitch the whole function calls to simulate the whole
// process of processing gRPC/HTTP2 events. This process is part of SocketTraceConnector's data
// transferring, but that process involves way more complexity. These facts signal misalignment
// between testability and higher-level logic structure.
//
// Tests that stitching and inflation specify time span correctly.
TEST(EventsTimeSpanTest, FromEventsToHTTP2Message) {
  EventParser parser;
  constexpr uint8_t flags = 0;
  constexpr uint32_t stream_id = 1;
  const std::string frame0 = PackHeadersFrame("\x86\x83", flags, stream_id);
  const std::string frame1 = PackContinuationFrame("\x88", NGHTTP2_FLAG_END_HEADERS, stream_id);
  const std::string frame2 = PackDataFrame("abc", NGHTTP2_FLAG_END_STREAM, stream_id);

  SocketDataEvent event0 = DataEventWithTimestamp(frame0, 0);
  SocketDataEvent event1 = DataEventWithTimestamp(frame1, 2);
  SocketDataEvent event2 = DataEventWithTimestamp(frame2, 4);

  parser.Append(event0);
  parser.Append(event1);
  parser.Append(event2);

  std::deque<Frame> frames;
  ParseResult<BufferPosition> res = parser.ParseFrames(MessageType::kUnknown, &frames);
  EXPECT_EQ(ParseState::kSuccess, res.state);
  EXPECT_THAT(frames, ElementsAre(MatchesTypePayload(NGHTTP2_HEADERS, "\x86\x83"),
                                  MatchesTypePayload(NGHTTP2_CONTINUATION, "\x88"),
                                  MatchesTypePayload(NGHTTP2_DATA, "abc")));
  Inflater inflater;
  StitchAndInflateHeaderBlocks(inflater.inflater(), &frames);

  std::map<uint32_t, HTTP2Message> stream_msgs;
  EXPECT_EQ(ParseState::kSuccess, StitchFramesToGRPCMessages(frames, &stream_msgs));
  ASSERT_THAT(stream_msgs, ElementsAre(Pair(1, _)));
  EXPECT_EQ(4, stream_msgs[1].timestamp_ns);
}

}  // namespace http2
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
