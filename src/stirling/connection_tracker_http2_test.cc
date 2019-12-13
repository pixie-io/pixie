#include "src/stirling/connection_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include "src/common/base/types.h"
#include "src/stirling/mysql/test_utils.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using ConnectionTrackerHTTP2Test = testing::EventsFixture;

TEST_F(ConnectionTrackerHTTP2Test, Basic) {
  ConnectionTracker tracker;

  {
    HTTP2DataEvent data_frame;
    data_frame.attr.ftype = DataFrameEventType::kDataFrameEventWrite;
    data_frame.attr.timestamp_ns = 11223344;
    data_frame.attr.conn_id.upid.pid = 123;
    data_frame.attr.conn_id.upid.start_time_ticks = 11000000;
    data_frame.attr.conn_id.fd = 5;
    data_frame.attr.traffic_class.protocol = kProtocolHTTP2;
    data_frame.attr.traffic_class.role = kRoleRequestor;
    data_frame.attr.stream_id = 7;
    data_frame.payload = "Request";

    tracker.AddHTTP2Data(data_frame);
  }

  {
    HTTP2DataEvent data_frame;
    data_frame.attr.ftype = DataFrameEventType::kDataFrameEventRead;
    data_frame.attr.timestamp_ns = 11223355;
    data_frame.attr.conn_id.upid.pid = 123;
    data_frame.attr.conn_id.upid.start_time_ticks = 11000000;
    data_frame.attr.conn_id.fd = 5;
    data_frame.attr.traffic_class.protocol = kProtocolHTTP2;
    data_frame.attr.traffic_class.role = kRoleRequestor;
    data_frame.attr.stream_id = 7;
    data_frame.payload = "Response";

    tracker.AddHTTP2Data(data_frame);
  }

  std::vector<http2::NewRecord> req_resp_pairs = tracker.ProcessMessages<http2::NewRecord>();

  EXPECT_EQ(req_resp_pairs.size(), 1);
}

}  // namespace stirling
}  // namespace pl
