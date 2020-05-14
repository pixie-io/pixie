#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/shared/metadata/metadata_state.h"

namespace pl {
namespace md {

TEST(K8sMetadataStateTest, CloneCopiedCIDR) {
  K8sMetadataState state;

  CIDRBlock pod_cidr0;
  CIDRBlock pod_cidr1;
  ASSERT_OK(ParseCIDRBlock("1.2.3.4/10", &pod_cidr0));
  ASSERT_OK(ParseCIDRBlock("16.17.18.19/10", &pod_cidr1));
  std::vector<CIDRBlock> pod_cidrs = {pod_cidr0, pod_cidr1};
  state.set_pod_cidrs(pod_cidrs);

  CIDRBlock service_cidr;
  ASSERT_OK(ParseCIDRBlock("10.64.0.0/16", &service_cidr));
  state.set_service_cidr(service_cidr);

  auto state_copy = state.Clone();

  auto& state_copy_pod_cidrs = state_copy->pod_cidrs();
  ASSERT_EQ(state_copy_pod_cidrs.size(), 2);
  EXPECT_EQ(pod_cidrs[0].ip_addr.AddrStr(), state_copy_pod_cidrs[0].ip_addr.AddrStr());
  EXPECT_EQ(pod_cidrs[0].prefix_length, state_copy_pod_cidrs[0].prefix_length);
  EXPECT_EQ(pod_cidrs[1].ip_addr.AddrStr(), state_copy_pod_cidrs[1].ip_addr.AddrStr());
  EXPECT_EQ(pod_cidrs[1].prefix_length, state_copy_pod_cidrs[1].prefix_length);

  ASSERT_TRUE(state_copy->service_cidr().has_value());
  EXPECT_EQ(service_cidr.ip_addr.AddrStr(), state_copy->service_cidr()->ip_addr.AddrStr());
  EXPECT_EQ(service_cidr.prefix_length, state_copy->service_cidr()->prefix_length);
}

}  // namespace md
}  // namespace pl
