#include "src/stirling/connection_stats.h"

#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

TEST(HashTest, CanBeUsedInFlatHashMap) {
  absl::flat_hash_map<ConnectionStats::AggKey, int> map;
  EXPECT_THAT(map, IsEmpty());

  ConnectionStats::AggKey key = {
      .upid = {.tgid = 1, .start_time_ticks = 2},
      .remote_addr = "test",
      .remote_port = 12345,
  };

  map[key] = 1;
  EXPECT_THAT(map, SizeIs(1));
  map[key] = 2;
  EXPECT_THAT(map, SizeIs(1));

  ConnectionStats::AggKey key_diff_upid = key;
  key_diff_upid.upid = {.tgid = 1, .start_time_ticks = 3},

  map[key_diff_upid] = 1;
  EXPECT_THAT(map, SizeIs(2));
  map[key] = 2;
  EXPECT_THAT(map, SizeIs(2));
}

class ConnectionStatsTest : public ::testing::Test {
 protected:
  ConnectionStats conn_stats_;
};

auto AggKeyIs(int tgid, std::string_view remote_addr) {
  return AllOf(Field(&ConnectionStats::AggKey::upid, Field(&upid_t::tgid, tgid)),
               Field(&ConnectionStats::AggKey::remote_addr, remote_addr));
}

auto StatsIs(int open, int close, int sent, int recv) {
  return AllOf(Field(&ConnectionStats::Stats::conn_open, open),
               Field(&ConnectionStats::Stats::conn_close, close),
               Field(&ConnectionStats::Stats::bytes_sent, sent),
               Field(&ConnectionStats::Stats::bytes_recv, recv));
}

// Tests that aggregated records for client side events are correctly put into ConnectionStats.
TEST_F(ConnectionStatsTest, ClientSizeAggregationRecord) {
  struct conn_id_t conn_id = {
      .upid = {.tgid = 1, .start_time_ticks = 2},
      .fd = 2,
      .tsid = 3,
  };

  // GCC requires initialize all fields in designated initializer expression.
  // So we use the field initialization.
  struct conn_event_t event = {};
  event.conn_id = conn_id;
  auto* sockaddr = reinterpret_cast<struct sockaddr_in*>(&event.addr);
  sockaddr->sin_family = AF_INET;
  sockaddr->sin_port = 12345;
  // 1.1.1.1
  sockaddr->sin_addr.s_addr = 0x01010101;

  struct socket_control_event_t ctrl_event = {};
  ctrl_event.type = kConnOpen, ctrl_event.open = event;

  // This setup the remote address and port, which is then used by ConnectionStats::AddDataEvent().
  ConnectionTracker tracker;
  tracker.AddControlEvent(ctrl_event);

  SocketDataEvent data_event;
  data_event.attr = {};
  data_event.attr.conn_id = conn_id;
  data_event.attr.traffic_class.protocol = kProtocolHTTP;
  data_event.attr.traffic_class.role = kRoleClient;
  data_event.attr.direction = kEgress;
  data_event.attr.msg_size = 12345;

  conn_stats_.AddDataEvent(tracker, data_event);
  conn_stats_.AddDataEvent(tracker, data_event);

  data_event.attr.direction = kIngress;
  conn_stats_.AddDataEvent(tracker, data_event);
  conn_stats_.AddDataEvent(tracker, data_event);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              ElementsAre(Pair(AggKeyIs(1, "1.1.1.1"), StatsIs(1, 0, 24690, 24690))));

  auto data_event_cpy = std::make_unique<SocketDataEvent>(data_event);
  // This changes tracker's traffic class to be consistent with conn_stats_.
  tracker.AddDataEvent(std::move(data_event_cpy));

  conn_stats_.AddConnCloseEvent(tracker);
  // Tests that after receiving conn close event for a connection, another same close event wont
  // increment the connection.
  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              ElementsAre(Pair(AggKeyIs(1, "1.1.1.1"), StatsIs(1, 1, 24690, 24690))));

  conn_stats_.AddConnCloseEvent(tracker);
  // The conn_close is not incremented.
  EXPECT_THAT(conn_stats_.mutable_agg_stats(),
              ElementsAre(Pair(AggKeyIs(1, "1.1.1.1"), StatsIs(1, 1, 24690, 24690))));
}

// Tests that disabled ConnectionTracker causes data event being ignored.
TEST_F(ConnectionStatsTest, DisabledConnectionTracker) {
  struct conn_id_t conn_id = {
      .upid = {.tgid = 1, .start_time_ticks = 2},
      .fd = 2,
      .tsid = 3,
  };

  // This setup the remote address and port, which is then used by ConnectionStats::AddDataEvent().
  ConnectionTracker tracker;
  tracker.Disable("test");

  SocketDataEvent data_event;
  data_event.attr = {};
  data_event.attr.conn_id = conn_id;
  data_event.attr.traffic_class.protocol = kProtocolHTTP;
  data_event.attr.traffic_class.role = kRoleClient;
  data_event.attr.direction = kEgress;
  data_event.attr.msg_size = 12345;

  conn_stats_.AddDataEvent(tracker, data_event);

  EXPECT_THAT(conn_stats_.mutable_agg_stats(), IsEmpty());
}

}  // namespace stirling
}  // namespace pl
