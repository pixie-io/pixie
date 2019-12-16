#include "src/stirling/connection_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/types.h"
#include "src/stirling/testing/events_fixture.h"
#include "src/stirling/testing/http2_stream_generator.h"

namespace pl {
namespace stirling {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using ConnectionTrackerHTTP2Test = testing::EventsFixture;

TEST_F(ConnectionTrackerHTTP2Test, BasicData) {
  ConnectionTracker tracker;

  auto frame_generator = testing::StreamEventGenerator(upid_t{{123}, 11000000}, 5, 7);
  std::unique_ptr<HTTP2DataEvent> data_frame;

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Request");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Response");
  tracker.AddHTTP2Data(std::move(data_frame));

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Response");
}

TEST_F(ConnectionTrackerHTTP2Test, BasicHeader) {
  ConnectionTracker tracker;

  auto frame_generator = testing::StreamEventGenerator(upid_t{{123}, 11000000}, 5, 7);
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(std::move(header_event));

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_THAT(records[0].send.headers, UnorderedElementsAre(Pair(":method", "post")));
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

TEST_F(ConnectionTrackerHTTP2Test, MultipleDataFrames) {
  ConnectionTracker tracker;

  auto frame_generator = testing::StreamEventGenerator(upid_t{{123}, 11000000}, 5, 7);
  std::unique_ptr<HTTP2DataEvent> data_frame;

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(std::move(data_frame));

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Response");
}

TEST_F(ConnectionTrackerHTTP2Test, MixedHeadersAndData) {
  ConnectionTracker tracker;

  auto frame_generator = testing::StreamEventGenerator(upid_t{{123}, 11000000}, 5, 7);
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

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(std::move(data_frame));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(std::move(header_event));

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data, "Request");
  EXPECT_EQ(records[0].recv.data, "Response");
  EXPECT_THAT(records[0].send.headers,
              UnorderedElementsAre(Pair(":method", "post"), Pair(":host", "pixie.ai"),
                                   Pair(":path", "/magic")));
  EXPECT_THAT(records[0].recv.headers, UnorderedElementsAre(Pair(":status", "200")));
}

}  // namespace stirling
}  // namespace pl
