#include "src/stirling/connection_stats.h"

#include <absl/container/flat_hash_map.h>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::testing::IsEmpty;
using ::testing::SizeIs;

TEST(HashTest, CanBeUsedInFlatHashMap) {
  absl::flat_hash_map<ConnectionStats::AggKey, int> map;
  EXPECT_THAT(map, IsEmpty());

  ConnectionStats::AggKey key = {
      .upid = {.tgid = 1, .start_time_ticks = 2},
      .traffic_class = {.protocol = kProtocolHTTP, .role = kRoleClient},
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

namespace internal {

TEST(BuildAggKeyTest, UPIDClearedForClientRole) {
  upid_t upid = {.tgid = 1, .start_time_ticks = 2};
  traffic_class_t traffic_class = {.protocol = kProtocolHTTP, .role = kRoleClient};
  std::string remote_addr = "localhost";
  int remote_port = 12345;
  ConnectionStats::AggKey key = BuildAggKey(upid, traffic_class, remote_addr, remote_port);
  EXPECT_EQ(0, key.upid.tgid);
  EXPECT_EQ(0, key.upid.start_time_ticks);
  EXPECT_EQ(kProtocolHTTP, key.traffic_class.protocol);
  EXPECT_EQ(kRoleClient, key.traffic_class.role);
  EXPECT_EQ("localhost", key.remote_addr);
  EXPECT_EQ(12345, key.remote_port);
}

TEST(BuildAggKeyTest, RemoteAddrPortClearedForServerRole) {
  upid_t upid = {.tgid = 1, .start_time_ticks = 2};
  traffic_class_t traffic_class = {.protocol = kProtocolHTTP, .role = kRoleServer};
  std::string remote_addr = "localhost";
  int remote_port = 12345;
  ConnectionStats::AggKey key = BuildAggKey(upid, traffic_class, remote_addr, remote_port);
  EXPECT_EQ(1, key.upid.tgid);
  EXPECT_EQ(2, key.upid.start_time_ticks);
  EXPECT_EQ(kProtocolHTTP, key.traffic_class.protocol);
  EXPECT_EQ(kRoleServer, key.traffic_class.role);
  EXPECT_EQ("", key.remote_addr);
  EXPECT_EQ(0, key.remote_port);
}

TEST(BuildAggKeyTest, RoleAndProtocolCanBeUnknown) {
  upid_t upid = {.tgid = 1, .start_time_ticks = 2};
  traffic_class_t traffic_class = {.protocol = kProtocolUnknown, .role = kRoleUnknown};
  std::string remote_addr = "localhost";
  int remote_port = 12345;
  ConnectionStats::AggKey key = BuildAggKey(upid, traffic_class, remote_addr, remote_port);
  EXPECT_EQ(1, key.upid.tgid);
  EXPECT_EQ(2, key.upid.start_time_ticks);
  EXPECT_EQ(kProtocolUnknown, key.traffic_class.protocol);
  EXPECT_EQ(kRoleUnknown, key.traffic_class.role);
  EXPECT_EQ("localhost", key.remote_addr);
  EXPECT_EQ(12345, key.remote_port);
}

}  // namespace internal

}  // namespace stirling
}  // namespace pl
