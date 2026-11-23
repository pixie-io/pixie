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

#include <gtest/gtest.h>

#include <memory>
#include <utility>
#include <vector>

#include "src/stirling/core/connector_context.h"

#include "src/common/event/real_time_system.h"
#include "src/common/testing/testing.h"

using ::px::testing::BazelRunfilePath;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace px {
namespace stirling {

// Tests that the UPIDs from the local proc path are detected.
TEST(SystemWideStandaloneContextTest, ListUPIDs) {
  const std::filesystem::path proc_path = BazelRunfilePath("src/common/system/testdata/proc");
  SystemWideStandaloneContext ctx(proc_path);
  EXPECT_THAT(ctx.GetUPIDs(),
              UnorderedElementsAre(md::UPID{0, 123, 14329}, md::UPID{0, 1, 13},
                                   md::UPID{0, 456, 17594622}, md::UPID{0, 789, 46120203}));
}

TEST(AgentContextTest, ForwardsProcessParents) {
  event::RealTimeSystem time_system;
  auto state = std::make_shared<md::AgentMetadataState>("myhost", 1, 123, sole::uuid4(), "mypod",
                                                        sole::uuid4(), "myvizier",
                                                        "myviziernamespace", &time_system);
  std::vector<std::pair<md::UPID, md::UPID>> parents;
  AgentContext ctx(state, [&parents](md::UPID upid, md::UPID parent_upid) {
    parents.emplace_back(upid, parent_upid);
  });

  ctx.AddProcessParent(md::UPID{1, 300, 3000}, md::UPID{1, 100, 1000});

  EXPECT_THAT(parents, ElementsAre(Pair(md::UPID{1, 300, 3000}, md::UPID{1, 100, 1000})));
}

TEST(AgentContextTest, ReportsProcessCgroup) {
  event::RealTimeSystem time_system;
  auto state = std::make_shared<md::AgentMetadataState>("myhost", 1, 123, sole::uuid4(), "mypod",
                                                        sole::uuid4(), "myvizier",
                                                        "myviziernamespace", &time_system);
  std::vector<std::pair<md::UPID, uint64_t>> cgroups;
  AgentContext ctx(state, nullptr, [&cgroups](md::UPID upid, uint64_t cgroup_id) {
    cgroups.emplace_back(upid, cgroup_id);
  });

  ctx.AddProcessCgroup(md::UPID{1, 300, 3000}, 77);

  EXPECT_THAT(cgroups, ElementsAre(Pair(md::UPID{1, 300, 3000}, 77)));
}

}  // namespace stirling
}  // namespace px
