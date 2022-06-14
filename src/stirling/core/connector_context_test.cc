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

#include "src/stirling/core/connector_context.h"

#include "src/common/testing/testing.h"

using ::px::testing::BazelRunfilePath;
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

}  // namespace stirling
}  // namespace px
