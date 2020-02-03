#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/shared/metadata/metadata_state.h"

namespace pl {
namespace md {

TEST(K8sMetadataStateTest, CloneCopiedClusterCIDR) {
  K8sMetadataState state;
  CIDRBlock block;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/10", &block));
  state.set_cluster_cidr(block);
  auto state_copy = state.Clone();
  ASSERT_TRUE(state_copy->cluster_cidr().has_value());
  EXPECT_EQ(block.ip_addr.AddrStr(), state_copy->cluster_cidr()->ip_addr.AddrStr());
  EXPECT_EQ(block.prefix_length, state_copy->cluster_cidr()->prefix_length);
}

}  // namespace md
}  // namespace pl
