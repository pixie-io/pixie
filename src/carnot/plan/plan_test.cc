#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/plan/plan.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/proto/test_proto.h"

namespace pl {
namespace carnot {
namespace plan {

class PlanWalkerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    carnotpb::Plan plan_pb;
    ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(
        carnotpb::testutils::kPlanWithFiveNodes, &plan_pb));
    ASSERT_OK(plan_.Init(plan_pb));
  }
  Plan plan_;
};

TEST_F(PlanWalkerTest, basic_tests) {
  EXPECT_EQ(plan_.nodes().at(1)->id(), 1);
  std::vector<int64_t> pf_order;
  auto s = PlanWalker()
               .OnPlanFragment([&](auto* pf) {
                 pf_order.push_back(pf->id());
                 return Status::OK();
               })
               .Walk(&plan_);
  EXPECT_OK(s);
  EXPECT_EQ(std::vector<int64_t>({1, 2, 3, 4, 5}), pf_order);
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
