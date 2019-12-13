#include "src/stirling/connection_tracker.h"

#include <gtest/gtest.h>

#include "src/common/base/types.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ConnectionTrackerHTTP2Test = testing::EventsFixture;

class StreamFrameGenerator {
 public:
  StreamFrameGenerator(ReqRespRole role, upid_t upid, uint32_t fd, uint32_t stream_id) {
    base_frame_.attr.conn_id.upid = upid;
    base_frame_.attr.conn_id.fd = fd;
    base_frame_.attr.stream_id = stream_id;
    base_frame_.attr.traffic_class.protocol = kProtocolHTTP2;
    base_frame_.attr.traffic_class.role = role;
  }

  template <DataFrameEventType TType>
  HTTP2DataEvent GenFrame(std::string_view body) {
    HTTP2DataEvent frame = base_frame_;
    frame.attr.ftype = TType;
    frame.attr.timestamp_ns = ++ts_;
    frame.attr.data_len = body.length();
    frame.payload = body;
    return frame;
  }

 private:
  HTTP2DataEvent base_frame_;
  uint64_t ts_ = 0;
};

TEST_F(ConnectionTrackerHTTP2Test, Basic) {
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

}  // namespace stirling
}  // namespace pl
