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

#include "src/stirling/source_connectors/proc_exit/proc_exit_connector.h"

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {

// Tests that ProcExitConnector::Create() works as expected.
TEST(ProcExitConnectorTest, Create) {
  auto connector = ProcExitConnector::Create("test_proc_exit_connector");
  ASSERT_TRUE(connector != nullptr);
  EXPECT_OK(connector->Init());
}

}  // namespace stirling
}  // namespace px
