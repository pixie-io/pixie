#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/shared/metadata/metadata_state.h"

namespace pl {
namespace md {

TEST(K8sMetadataStateTest, CloneCopiedCIDR) {
  K8sMetadataState state;

  CIDRBlock cluster_cidr;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/10", &cluster_cidr));
  state.set_cluster_cidr(cluster_cidr);

  CIDRBlock service_cidr;
  ASSERT_OK(ParseCIDRBlock("10.64.0.0/16", &service_cidr));
  state.set_service_cidr(service_cidr);

  auto state_copy = state.Clone();

  ASSERT_TRUE(state_copy->cluster_cidr().has_value());
  EXPECT_EQ(cluster_cidr.ip_addr.AddrStr(), state_copy->cluster_cidr()->ip_addr.AddrStr());
  EXPECT_EQ(cluster_cidr.prefix_length, state_copy->cluster_cidr()->prefix_length);

  ASSERT_TRUE(state_copy->service_cidr().has_value());
  EXPECT_EQ(service_cidr.ip_addr.AddrStr(), state_copy->service_cidr()->ip_addr.AddrStr());
  EXPECT_EQ(service_cidr.prefix_length, state_copy->service_cidr()->prefix_length);
}

}  // namespace md
}  // namespace pl
