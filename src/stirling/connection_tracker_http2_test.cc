#include "src/stirling/connection_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/types.h"
#include "src/stirling/testing/http2_stream_generator.h"

namespace pl {
namespace stirling {

namespace http2 = protocols::http2;

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class ConnectionTrackerHTTP2Test : public ::testing::Test {
 protected:
  testing::RealClock real_clock_;
};

TEST_F(ConnectionTrackerHTTP2Test, BasicData) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Response", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Response");
}

TEST_F(ConnectionTrackerHTTP2Test, BasicHeader) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventWrite>();
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
  tracker.AddHTTP2Header(std::move(header_event));

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_THAT(records[0].send.headers, UnorderedElementsAre(Pair(":method", "post")));
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

TEST_F(ConnectionTrackerHTTP2Test, MultipleDataFrames) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Response");
}

TEST_F(ConnectionTrackerHTTP2Test, MixedHeadersAndData) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
  tracker.AddHTTP2Header(std::move(header_event));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(std::move(data_frame));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
  tracker.AddHTTP2Header(std::move(header_event));

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Response");
  EXPECT_THAT(records[0].send.headers,
              UnorderedElementsAre(Pair(":method", "post"), Pair(":host", "pixie.ai"),
                                   Pair(":path", "/magic")));
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

// This test models capturing data mid-stream, where we may have missed the request headers.
TEST_F(ConnectionTrackerHTTP2Test, MidStreamCapture) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  // Note that request headers are missing.

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(std::move(data_frame));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
  tracker.AddHTTP2Header(std::move(header_event));

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_THAT(records[0].send.data, StrEq("Request"));
  EXPECT_THAT(records[0].recv.data, StrEq("Response"));
  EXPECT_THAT(records[0].send.headers, IsEmpty());
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

// This test ensures we ignore data with file descriptor of zero
// which is usually indicative of something erroneous.
// With uprobes, in particular, it implies we failed to get the net.Conn data,
// which could be because we have currently hard-coded dwarf info values.
TEST_F(ConnectionTrackerHTTP2Test, ZeroFD) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 0, .tsid = 3};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
  tracker.AddHTTP2Header(std::move(header_event));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true);
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(std::move(data_frame));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
  tracker.AddHTTP2Header(std::move(header_event));

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  EXPECT_TRUE(records.empty());
}

TEST_F(ConnectionTrackerHTTP2Test, HTTP2StreamsCleanedUpAfterBreachingSizeLimit) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  auto header_event1 = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  auto header_event2 = frame_generator.GenHeader<kHeaderEventWrite>(":scheme", "https");
  // Events with even stream IDs are put on recv_data_.
  header_event2->attr.stream_id = 8;

  FLAGS_messages_size_limit_bytes = 10000;
  tracker.AddHTTP2Header(std::move(header_event1));
  tracker.AddHTTP2Header(std::move(header_event2));
  tracker.ProcessToRecords<http2::ProtocolTraits>();

  EXPECT_THAT(tracker.http2_send_streams(), ::testing::SizeIs(1));
  EXPECT_THAT(tracker.http2_recv_streams(), ::testing::SizeIs(1));

  FLAGS_messages_size_limit_bytes = 0;
  tracker.ProcessToRecords<http2::ProtocolTraits>();
  EXPECT_THAT(tracker.http2_send_streams(), ::testing::IsEmpty());
  EXPECT_THAT(tracker.http2_recv_streams(), ::testing::IsEmpty());
}

TEST_F(ConnectionTrackerHTTP2Test, HTTP2StreamsCleanedUpAfterExpiration) {
  ConnectionTracker tracker;

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 5, .tsid = 0};
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);
  auto header_event1 = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  auto header_event2 = frame_generator.GenHeader<kHeaderEventWrite>(":scheme", "https");
  // Change the second to be a different stream ID.
  header_event2->attr.stream_id = 8;

  FLAGS_messages_size_limit_bytes = 10000;
  FLAGS_messages_expiration_duration_secs = 10000;
  tracker.AddHTTP2Header(std::move(header_event1));
  tracker.AddHTTP2Header(std::move(header_event2));
  tracker.ProcessToRecords<http2::ProtocolTraits>();

  EXPECT_THAT(tracker.http2_send_streams(), ::testing::SizeIs(1));
  EXPECT_THAT(tracker.http2_recv_streams(), ::testing::SizeIs(1));

  FLAGS_messages_expiration_duration_secs = 0;
  tracker.ProcessToRecords<http2::ProtocolTraits>();
  EXPECT_THAT(tracker.http2_send_streams(), ::testing::IsEmpty());
  EXPECT_THAT(tracker.http2_recv_streams(), ::testing::IsEmpty());
}

TEST_F(ConnectionTrackerHTTP2Test, StreamIDJumpAhead) {
  ConnectionTracker tracker;

  // The first stream is ordinary.
  {
    const conn_id_t kConnID = {
        .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 3, .tsid = 21};
    const uint32_t kStreamID = 7;
    auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
    tracker.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Response");
    tracker.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker.AddHTTP2Header(std::move(header_event));
  }

  // And now another stream which jumps stream IDs for some reason.
  // This stream ID has to have the same even/oddness.
  {
    const conn_id_t kConnID = {
        .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 3, .tsid = 21};
    const uint32_t kStreamID = 100007;
    auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/wormhole");
    tracker.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Long ways from home");
    tracker.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker.AddHTTP2Header(std::move(header_event));
  }

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Long ways from home");
  EXPECT_THAT(records[0].send.headers,
              UnorderedElementsAre(Pair(":method", "post"), Pair(":host", "pixie.ai"),
                                   Pair(":path", "/wormhole")));
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

TEST_F(ConnectionTrackerHTTP2Test, StreamIDJumpBack) {
  ConnectionTracker tracker;

  // The first stream is ordinary.
  {
    const conn_id_t kConnID = {
        .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 3, .tsid = 21};
    const uint32_t kStreamID = 100007;
    auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
    tracker.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Response");
    tracker.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker.AddHTTP2Header(std::move(header_event));
  }

  // And now another stream which jumps stream IDs for some reason.
  // This stream ID has to have the same even/oddness.
  {
    const conn_id_t kConnID = {
        .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 3, .tsid = 21};
    const uint32_t kStreamID = 7;
    auto frame_generator = testing::StreamEventGenerator(&real_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    LOG(INFO) << "A";
    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker.AddHTTP2Header(std::move(header_event));
    LOG(INFO) << "B";

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/wormhole");
    tracker.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Long ways from home");
    tracker.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker.AddHTTP2Header(std::move(header_event));
  }

  std::vector<http2::Record> records = tracker.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Long ways from home");
  EXPECT_THAT(records[0].send.headers,
              UnorderedElementsAre(Pair(":method", "post"), Pair(":host", "pixie.ai"),
                                   Pair(":path", "/wormhole")));
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

}  // namespace stirling
}  // namespace pl
