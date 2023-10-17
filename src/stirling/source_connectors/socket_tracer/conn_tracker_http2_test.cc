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

#include "src/stirling/source_connectors/socket_tracer/conn_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/types.h"
#include "src/stirling/source_connectors/socket_tracer/testing/http2_stream_generator.h"

namespace px {
namespace stirling {

namespace http2 = protocols::http2;

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class ConnTrackerHTTP2Test : public ::testing::Test {
 protected:
  void SetUp() override { tracker_.SetConnID(kConnID); }

  std::chrono::steady_clock::time_point now() {
    return testing::NanosToTimePoint(mock_clock_.now());
  }

  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 3, .tsid = 21};

  ConnTracker tracker_;
  testing::MockClock mock_clock_;
};

auto EqHTTP2HalfStream(const protocols::http2::HalfStream& x) {
  return AllOf(Property(&protocols::http2::HalfStream::end_stream, Eq(x.end_stream())),
               Property(&protocols::http2::HalfStream::data, StrEq(x.data())),
               Property(&protocols::http2::HalfStream::headers, Eq(x.headers())),
               Property(&protocols::http2::HalfStream::trailers, Eq(x.trailers())));
}

auto EqHTTP2Record(const protocols::http2::Stream& x) {
  return AllOf(Field(&protocols::http2::Stream::recv, EqHTTP2HalfStream(x.recv)),
               Field(&protocols::http2::Stream::send, EqHTTP2HalfStream(x.send)));
}

TEST_F(ConnTrackerHTTP2Test, BasicData) {
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);

  tracker_.AddHTTP2Header(frame_generator.GenHeader<kHeaderEventWrite>(":method", "POST"));
  tracker_.AddHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true));

  tracker_.AddHTTP2Data(
      frame_generator.GenDataFrame<kDataFrameEventRead>("Response", /* end_stream */ true));
  tracker_.AddHTTP2Header(frame_generator.GenHeader<kHeaderEventRead>(":status", "200"));

  std::vector<http2::Record> records = tracker_.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data(), "Request");
  EXPECT_EQ(records[0].recv.data(), "Response");
}

TEST_F(ConnTrackerHTTP2Test, BasicHeader) {
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker_.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventWrite>();
  tracker_.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker_.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
  tracker_.AddHTTP2Header(std::move(header_event));

  std::vector<http2::Record> records = tracker_.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_THAT(records[0].send.headers(), UnorderedElementsAre(Pair(":method", "post")));
  EXPECT_THAT(records[0].recv.headers(), UnorderedElementsAre(Pair(":status", "200")));
}

// Tests that multiple data frames are exported to records.
TEST_F(ConnTrackerHTTP2Test, MultipleDataFrames) {
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker_.AddHTTP2Header(std::move(header_event));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker_.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true);
  tracker_.AddHTTP2Data(std::move(data_frame));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker_.AddHTTP2Header(std::move(header_event));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker_.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse", /* end_stream */ true);
  tracker_.AddHTTP2Data(std::move(data_frame));

  // Set death count down to 1 so it is treated as the last iteration.
  tracker_.MarkForDeath(1);
  std::vector<http2::Record> records = tracker_.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data(), "Request");
  EXPECT_EQ(records[0].recv.data(), "Response");
}

TEST_F(ConnTrackerHTTP2Test, MixedHeadersAndData) {
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);
  std::unique_ptr<HTTP2DataEvent> data_frame;
  std::unique_ptr<HTTP2HeaderEvent> header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker_.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
  tracker_.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
  tracker_.AddHTTP2Header(std::move(header_event));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("Req");
  tracker_.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventWrite>("uest", /* end_stream */ true);
  tracker_.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Resp");
  tracker_.AddHTTP2Data(std::move(data_frame));

  data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("onse");
  tracker_.AddHTTP2Data(std::move(data_frame));

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker_.AddHTTP2Header(std::move(header_event));

  header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
  tracker_.AddHTTP2Header(std::move(header_event));

  std::vector<http2::Record> records = tracker_.ProcessToRecords<http2::ProtocolTraits>();

  ASSERT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].send.data(), "Request");
  EXPECT_EQ(records[0].recv.data(), "Response");
  EXPECT_THAT(records[0].send.headers(),
              UnorderedElementsAre(Pair(":method", "post"), Pair(":host", "pixie.ai"),
                                   Pair(":path", "/magic")));
  EXPECT_THAT(records[0].recv.headers(), UnorderedElementsAre(Pair(":status", "200")));
}

// This test ensures we ignore data with file descriptor of zero
// which is usually indicative of something erroneous.
// With uprobes, in particular, it implies we failed to get the net.Conn data,
// which could be because we have currently hard-coded dwarf info values.
TEST_F(ConnTrackerHTTP2Test, ZeroFD) {
  ConnTracker tracker;
  const conn_id_t kConnID = {
      .upid = {{.pid = 123}, .start_time_ticks = 11000000}, .fd = 0, .tsid = 21};
  tracker.SetConnID(kConnID);

  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);
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

TEST_F(ConnTrackerHTTP2Test, HTTP2StreamsCleanedUpAfterBreachingSizeLimit) {
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);
  auto header_event1 = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  auto header_event2 = frame_generator.GenHeader<kHeaderEventWrite>(":scheme", "https");
  // Events with even stream IDs are put on recv_data_.
  header_event2->attr.stream_id = 8;

  int frame_size_limit_bytes = 10000;
  int buffer_size_limit_bytes = 10000;
  auto frame_expiry_timestamp = now() - std::chrono::seconds(10000);
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);
  tracker_.AddHTTP2Header(std::move(header_event1));
  tracker_.AddHTTP2Header(std::move(header_event2));
  tracker_.ProcessToRecords<http2::ProtocolTraits>();
  tracker_.Cleanup<http2::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                          frame_expiry_timestamp, buffer_expiry_timestamp);

  EXPECT_EQ(tracker_.http2_client_streams_size(), 1);
  EXPECT_EQ(tracker_.http2_server_streams_size(), 1);

  frame_size_limit_bytes = 0;
  tracker_.ProcessToRecords<http2::ProtocolTraits>();
  tracker_.Cleanup<http2::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                          frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_EQ(tracker_.http2_client_streams_size(), 0);
  EXPECT_EQ(tracker_.http2_server_streams_size(), 0);
}

TEST_F(ConnTrackerHTTP2Test, HTTP2StreamsCleanedUpAfterExpiration) {
  const int kStreamID = 7;
  auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);
  auto header_event1 = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  auto header_event2 = frame_generator.GenHeader<kHeaderEventWrite>(":scheme", "https");
  // Change the second to be a different stream ID.
  header_event2->attr.stream_id = 8;

  int frame_size_limit_bytes = 10000;
  int buffer_size_limit_bytes = 10000;
  auto frame_expiry_timestamp = now() - std::chrono::seconds(10000);
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);
  tracker_.AddHTTP2Header(std::move(header_event1));
  tracker_.AddHTTP2Header(std::move(header_event2));
  tracker_.ProcessToRecords<http2::ProtocolTraits>();
  tracker_.Cleanup<http2::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                          frame_expiry_timestamp, buffer_expiry_timestamp);

  EXPECT_EQ(tracker_.http2_client_streams_size(), 1);
  EXPECT_EQ(tracker_.http2_server_streams_size(), 1);

  frame_expiry_timestamp = now();
  tracker_.ProcessToRecords<http2::ProtocolTraits>();
  tracker_.Cleanup<http2::ProtocolTraits>(frame_size_limit_bytes, buffer_size_limit_bytes,
                                          frame_expiry_timestamp, buffer_expiry_timestamp);
  EXPECT_EQ(tracker_.http2_client_streams_size(), 0);
  EXPECT_EQ(tracker_.http2_server_streams_size(), 0);
}

TEST_F(ConnTrackerHTTP2Test, StreamIDJumpAhead) {
  // The first stream is ordinary.
  {
    const uint32_t kStreamID = 7;
    auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
    tracker_.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker_.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Response");
    tracker_.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker_.AddHTTP2Header(std::move(header_event));
  }

  // And now another stream which jumps stream IDs for some reason.
  // This stream ID has to have the same even/oddness.
  {
    const uint32_t kStreamID = 100007;
    auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/wormhole");
    tracker_.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker_.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Long ways from home");
    tracker_.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker_.AddHTTP2Header(std::move(header_event));
  }

  std::vector<http2::Record> records = tracker_.ProcessToRecords<http2::ProtocolTraits>();

  http2::Record expected_record0;
  expected_record0.send.AddHeader(":method", "post");
  expected_record0.send.AddHeader(":host", "pixie.ai");
  expected_record0.send.AddHeader(":path", "/magic");
  expected_record0.send.AddData("Request");
  expected_record0.send.AddEndStream();
  expected_record0.recv.AddHeader(":status", "200");
  expected_record0.recv.AddData("Response");
  expected_record0.recv.AddEndStream();

  http2::Record expected_record1;
  expected_record1.send.AddHeader(":method", "post");
  expected_record1.send.AddHeader(":host", "pixie.ai");
  expected_record1.send.AddHeader(":path", "/wormhole");
  expected_record1.send.AddData("Request");
  expected_record1.send.AddEndStream();
  expected_record1.recv.AddHeader(":status", "200");
  expected_record1.recv.AddData("Long ways from home");
  expected_record1.recv.AddEndStream();

  EXPECT_THAT(records, UnorderedElementsAre(EqHTTP2Record(expected_record0),
                                            EqHTTP2Record(expected_record1)));
}

TEST_F(ConnTrackerHTTP2Test, StreamIDJumpBack) {
  // The first stream is ordinary.
  {
    const uint32_t kStreamID = 100007;
    auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
    tracker_.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker_.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Response");
    tracker_.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker_.AddHTTP2Header(std::move(header_event));
  }

  // And now another stream which jumps stream IDs for some reason.
  // This stream ID has to have the same even/oddness.
  {
    const uint32_t kStreamID = 7;
    auto frame_generator = testing::StreamEventGenerator(&mock_clock_, kConnID, kStreamID);

    std::unique_ptr<HTTP2DataEvent> data_frame;
    std::unique_ptr<HTTP2HeaderEvent> header_event;

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/wormhole");
    tracker_.AddHTTP2Header(std::move(header_event));

    data_frame =
        frame_generator.GenDataFrame<kDataFrameEventWrite>("Request", /* end_stream */ true);
    tracker_.AddHTTP2Data(std::move(data_frame));

    data_frame = frame_generator.GenDataFrame<kDataFrameEventRead>("Long ways from home");
    tracker_.AddHTTP2Data(std::move(data_frame));

    header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
    tracker_.AddHTTP2Header(std::move(header_event));

    header_event = frame_generator.GenEndStreamHeader<kHeaderEventRead>();
    tracker_.AddHTTP2Header(std::move(header_event));
  }

  std::vector<http2::Record> records = tracker_.ProcessToRecords<http2::ProtocolTraits>();

  http2::Record expected_record0;
  expected_record0.send.AddHeader(":method", "post");
  expected_record0.send.AddHeader(":host", "pixie.ai");
  expected_record0.send.AddHeader(":path", "/magic");
  expected_record0.send.AddData("Request");
  expected_record0.send.AddEndStream();
  expected_record0.recv.AddHeader(":status", "200");
  expected_record0.recv.AddData("Response");
  expected_record0.recv.AddEndStream();

  http2::Record expected_record1;
  expected_record1.send.AddHeader(":method", "post");
  expected_record1.send.AddHeader(":host", "pixie.ai");
  expected_record1.send.AddHeader(":path", "/wormhole");
  expected_record1.send.AddData("Request");
  expected_record1.send.AddEndStream();
  expected_record1.recv.AddHeader(":status", "200");
  expected_record1.recv.AddData("Long ways from home");
  expected_record1.recv.AddEndStream();

  EXPECT_THAT(records, UnorderedElementsAre(EqHTTP2Record(expected_record0),
                                            EqHTTP2Record(expected_record1)));
}

}  // namespace stirling
}  // namespace px
