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

#include "src/stirling/source_connectors/socket_tracer/protocols/mux/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"

namespace px {
namespace stirling {
namespace protocols {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

class StitchFramesTest : public ::testing::Test {
};

mux::Frame CreateMuxFrame(uint64_t ts_ns, mux::Type type, uint32_t tag) {
    mux::Frame frame;
    frame.timestamp_ns = ts_ns;
    frame.type = static_cast<int8_t>(type);
    frame.tag = tag;
    return frame;
}

TEST_F(StitchFramesTest, VerifySingleOutputMessage) {
    std::deque<mux::Frame> reqs = {
        // tinit check message
        CreateMuxFrame(0, mux::Type::RerrOld, 1),
        CreateMuxFrame(2, mux::Type::Tinit, 1),
        CreateMuxFrame(4, mux::Type::Tping, 1),
    };
    std::deque<mux::Frame> resps = {
        // tinit check message response
        CreateMuxFrame(1, mux::Type::RerrOld, 1),
        CreateMuxFrame(3, mux::Type::Rinit, 1),
        CreateMuxFrame(5, mux::Type::Rping, 1),
    };
    NoState state;

    RecordsWithErrorCount<mux::Record> res = mux::StitchFrames(&reqs, &resps, &state);
    EXPECT_EQ(res.error_count, 0);
    EXPECT_THAT(reqs, IsEmpty());
    EXPECT_THAT(resps, IsEmpty());
    PL_UNUSED(res);
}

TEST_F(StitchFramesTest, HandleParseErrResp) {
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
