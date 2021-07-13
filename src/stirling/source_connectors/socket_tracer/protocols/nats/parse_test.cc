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

#include "src/stirling/source_connectors/socket_tracer/protocols/nats/parse.h"

#include <string_view>
#include <vector>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace nats {

using ::testing::IsEmpty;
using ::testing::StrEq;

// Tests that the simple cases are as expected.
TEST(FindMessageBoundaryTest, ResultsAreAsExpected) {
  EXPECT_EQ(FindMessageBoundary(" CONNECT {} \r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" INFO {} \r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" PUB \r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" SUB \r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" UNSUB \r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" MSG \r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" PING\r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" PONG\r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" +OK\r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" -ERR 'test'\r\n", 0), 1);
  EXPECT_EQ(FindMessageBoundary(" {} \r\n", 0), std::string_view::npos);
}

}  // namespace nats
}  // namespace protocols
}  // namespace stirling
}  // namespace px
