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

#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"

#include <string>
#include <utility>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

TEST(IsMuxType, CanDetectMembershipOfSmallestAndLargestTypes) {
  // Smallest signed 8 bit value
  ASSERT_EQ(IsMuxType(static_cast<int8_t>(Type::kRerr)), true);
  // Largest signed 8 bit value
  ASSERT_EQ(IsMuxType(static_cast<int8_t>(Type::kRerrOld)), true);
}

TEST(GetMatchingRespType, ReturnsCorrespondingResponseTypes) {
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kRerrOld), Type::kRerrOld);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kRerr), Type::kRerr);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTinit), Type::kRinit);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTping), Type::kRping);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTdiscardedOld), Type::kRdiscarded);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTdiscarded), Type::kRdiscarded);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTdrain), Type::kRdrain);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTdispatch), Type::kRdispatch);
  ASSERT_OK_AND_EQ(GetMatchingRespType(Type::kTreq), Type::kRreq);
  ASSERT_NOT_OK(GetMatchingRespType(static_cast<Type>(0)));
}

TEST(Frame, BytesSize) {
  Frame f = {};
  f.why = "just because";
  f.InsertContext("usa", {{"ca", "san francisco"}, {"ny", "new york"}});
  f.InsertContext("canada", {{"qc", "montreal"}, {"on", "toronto"}});

  size_t s = std::string_view("just because").size() + std::string_view("usa").size() +
             std::string_view("canada").size() + std::string_view("ca").size() +
             std::string_view("ny").size() + std::string_view("qc").size() +
             std::string_view("on").size() + std::string_view("san francisco").size() +
             std::string_view("new york").size() + std::string_view("montreal").size() +
             std::string_view("toronto").size();

  EXPECT_EQ(f.ByteSize(), sizeof(f) + s);
}

}  // namespace mux
}  // namespace protocols
}  // namespace stirling
}  // namespace px
