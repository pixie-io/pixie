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
    ASSERT_EQ(IsMuxType(static_cast<int8_t>(Type::Rerr)), true);
    // Largest signed 8 bit value
    ASSERT_EQ(IsMuxType(static_cast<int8_t>(Type::RerrOld)), true);
}

TEST(GetMatchingRespType, A) {
    ASSERT_EQ(GetMatchingRespType(Type::RerrOld), Type::RerrOld);
    ASSERT_EQ(GetMatchingRespType(Type::Rerr), Type::Rerr);
    ASSERT_EQ(GetMatchingRespType(Type::Tinit), Type::Rinit);
    ASSERT_EQ(GetMatchingRespType(Type::Tping), Type::Rping);
    ASSERT_EQ(GetMatchingRespType(Type::TdiscardedOld), Type::Rdiscarded);
    ASSERT_EQ(GetMatchingRespType(Type::Tdiscarded), Type::Rdiscarded);
    ASSERT_EQ(GetMatchingRespType(Type::Tdrain), Type::Rdrain);
    ASSERT_EQ(GetMatchingRespType(Type::Tdispatch), Type::Rdispatch);
    ASSERT_EQ(GetMatchingRespType(Type::Treq), Type::Rreq);
}

}  // namespace mux
}  // namespace protocols
}  // namespace stirling
}  // namespace px
