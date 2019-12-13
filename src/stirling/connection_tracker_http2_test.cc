#include "src/stirling/connection_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/types.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using ConnectionTrackerHTTP2Test = testing::EventsFixture;

class StreamFrameGenerator {
 public:
  StreamFrameGenerator(ReqRespRole role, upid_t upid, uint32_t fd, uint32_t stream_id)
      : role_(role), upid_(upid), fd_(fd), stream_id_(stream_id) {}

  template <DataFrameEventType TType>
  HTTP2DataEvent GenFrame(std::string_view body) {
    HTTP2DataEvent frame;
    frame.attr.conn_id.upid = upid_;
    frame.attr.conn_id.fd = fd_;
    frame.attr.conn_id.generation = 0;
    frame.attr.stream_id = stream_id_;
    frame.attr.traffic_class.protocol = kProtocolHTTP2;
    frame.attr.traffic_class.role = role_;
    frame.attr.ftype = TType;
    frame.attr.timestamp_ns = ++ts_;
    frame.attr.data_len = body.length();
    frame.payload = body;
    return frame;
  }

  template <HeaderEventType TType>
  go_grpc_http2_header_event_t GenHeader(std::string_view name, std::string_view value) {
    go_grpc_http2_header_event_t hdr;
    hdr.conn_id.upid = upid_;
    hdr.conn_id.fd = fd_;
    hdr.conn_id.generation = 0;
    hdr.stream_id = stream_id_;
    hdr.traffic_class.protocol = kProtocolHTTP2;
    hdr.traffic_class.role = role_;
    hdr.htype = TType;
    hdr.timestamp_ns = ++ts_;
    strncpy(hdr.name.msg, name.data(), name.length());
    hdr.name.size = name.length();
    strncpy(hdr.value.msg, value.data(), value.length());
    hdr.value.size = value.length();
    return hdr;
  }

 private:
  ReqRespRole role_;
  upid_t upid_;
  uint32_t fd_;
  uint32_t stream_id_;

  uint64_t ts_ = 0;
};

TEST_F(ConnectionTrackerHTTP2Test, BasicData) {
  ConnectionTracker tracker;

  auto frame_generator = StreamFrameGenerator(kRoleRequestor, upid_t{{123}, 11000000}, 5, 7);
  HTTP2DataEvent data_frame;

  data_frame = frame_generator.GenFrame<kDataFrameEventWrite>("Request");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventRead>("Response");
  tracker.AddHTTP2Data(data_frame);

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].req.data, "Request");
  EXPECT_EQ(records[0].resp.data, "Response");
}

TEST_F(ConnectionTrackerHTTP2Test, BasicHeader) {
  ConnectionTracker tracker;

  auto frame_generator = StreamFrameGenerator(kRoleRequestor, upid_t{{123}, 11000000}, 5, 7);
  go_grpc_http2_header_event_t header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker.AddHTTP2Header(header_event);

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(header_event);

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_THAT(records[0].req.headers, UnorderedElementsAre(Pair(":method", "post")));
  EXPECT_THAT(records[0].resp.headers, UnorderedElementsAre(Pair(":status", "200")));
}

TEST_F(ConnectionTrackerHTTP2Test, MultipleDataFrames) {
  ConnectionTracker tracker;

  auto frame_generator = StreamFrameGenerator(kRoleRequestor, upid_t{{123}, 11000000}, 5, 7);
  HTTP2DataEvent data_frame;

  data_frame = frame_generator.GenFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventWrite>("uest");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(data_frame);

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].req.data, "Request");
  EXPECT_EQ(records[0].resp.data, "Response");
}

TEST_F(ConnectionTrackerHTTP2Test, MixedHeadersAndData) {
  ConnectionTracker tracker;

  auto frame_generator = StreamFrameGenerator(kRoleRequestor, upid_t{{123}, 11000000}, 5, 7);
  HTTP2DataEvent data_frame;
  go_grpc_http2_header_event_t header_event;

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":method", "post");
  tracker.AddHTTP2Header(header_event);

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":host", "pixie.ai");
  tracker.AddHTTP2Header(header_event);

  header_event = frame_generator.GenHeader<kHeaderEventWrite>(":path", "/magic");
  tracker.AddHTTP2Header(header_event);

  data_frame = frame_generator.GenFrame<kDataFrameEventWrite>("Req");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventWrite>("uest");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventRead>("Resp");
  tracker.AddHTTP2Data(data_frame);

  data_frame = frame_generator.GenFrame<kDataFrameEventRead>("onse");
  tracker.AddHTTP2Data(data_frame);

  header_event = frame_generator.GenHeader<kHeaderEventRead>(":status", "200");
  tracker.AddHTTP2Header(header_event);

  std::vector<http2::NewRecord> records = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(records.size(), 1);
  EXPECT_EQ(records[0].req.data, "Request");
  EXPECT_EQ(records[0].resp.data, "Response");
  EXPECT_THAT(records[0].req.headers,
              UnorderedElementsAre(Pair(":method", "post"), Pair(":host", "pixie.ai"),
                                   Pair(":path", "/magic")));
  EXPECT_THAT(records[0].resp.headers, UnorderedElementsAre(Pair(":status", "200")));
}

}  // namespace stirling
}  // namespace pl
