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

#include "src/carnot/plan/plan.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace plan {

class PlanWalkerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    planpb::Plan plan_pb;
    ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(planpb::testutils::kPlanWithFiveNodes,
                                                              &plan_pb));
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
}  // namespace px
