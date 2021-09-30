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

TEST_F(StitchFramesTest, VerifyTransmitReceivePairsAreMatched) {
    std::deque<mux::Frame> reqs = {
        // tinit check message
        CreateMuxFrame(0, mux::Type::RerrOld, 1),
        CreateMuxFrame(2, mux::Type::Tinit, 1),
        CreateMuxFrame(4, mux::Type::Tping, 1),
        CreateMuxFrame(6, mux::Type::Tdispatch, 1),
        CreateMuxFrame(8, mux::Type::TdiscardedOld, 1),
        CreateMuxFrame(10, mux::Type::Tdiscarded, 1),
        CreateMuxFrame(12, mux::Type::Treq, 1),
        CreateMuxFrame(14, mux::Type::Tdrain, 1),
    };
    std::deque<mux::Frame> resps = {
        // tinit check message response
        CreateMuxFrame(1, mux::Type::RerrOld, 1),
        CreateMuxFrame(3, mux::Type::Rinit, 1),
        CreateMuxFrame(5, mux::Type::Rping, 1),
        CreateMuxFrame(7, mux::Type::Rdispatch, 1),
        CreateMuxFrame(9, mux::Type::Rdiscarded, 1),
        CreateMuxFrame(11, mux::Type::Rdiscarded, 1),
        CreateMuxFrame(13, mux::Type::Rreq, 1),
        CreateMuxFrame(15, mux::Type::Rdrain, 1),
    };
    NoState state;

    RecordsWithErrorCount<mux::Record> res = mux::StitchFrames(&reqs, &resps, &state);
    EXPECT_EQ(res.error_count, 0);
    EXPECT_EQ(res.records.size(), 8);
    EXPECT_THAT(reqs, IsEmpty());
    EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, VerifyTleaseIsHandled) {
    std::deque<mux::Frame> reqs = {
        // tinit check message
        CreateMuxFrame(0, mux::Type::RerrOld, 1),
        CreateMuxFrame(2, mux::Type::Tlease, 1),
        CreateMuxFrame(4, mux::Type::Tping, 1),
    };
    std::deque<mux::Frame> resps = {
        // tinit check message response
        CreateMuxFrame(1, mux::Type::RerrOld, 1),
        CreateMuxFrame(5, mux::Type::Rping, 1),
    };
    NoState state;

    RecordsWithErrorCount<mux::Record> res = mux::StitchFrames(&reqs, &resps, &state);

    EXPECT_EQ(res.error_count, 0);
    EXPECT_EQ(res.records.size(), 3);
    EXPECT_EQ(mux::Type(res.records[1].req.type), mux::Type::Tlease);
    // There is no response for Tlease so the response frame should
    // have an uninitialized type field
    EXPECT_EQ(res.records[1].resp.type, 0);

    EXPECT_THAT(reqs, IsEmpty());
    EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, StaleResponsesAreHandled) {
    std::deque<mux::Frame> reqs = {
        // tinit check message
        CreateMuxFrame(1, mux::Type::RerrOld, 1),
    };
    std::deque<mux::Frame> resps = {
        // tinit check message response
        CreateMuxFrame(0, mux::Type::RerrOld, 1),
        CreateMuxFrame(2, mux::Type::RerrOld, 1),
    };
    NoState state;

    RecordsWithErrorCount<mux::Record> res = mux::StitchFrames(&reqs, &resps, &state);

    EXPECT_EQ(res.error_count, 0);
    EXPECT_EQ(res.records.size(), 2);
    EXPECT_EQ(mux::Type(res.records[0].resp.type), mux::Type::RerrOld);
    EXPECT_EQ(res.records[0].req.type, 0);

    EXPECT_THAT(reqs, IsEmpty());
    EXPECT_THAT(resps, IsEmpty());
}

TEST_F(StitchFramesTest, StaleRequestsAreHandled) {
    std::deque<mux::Frame> reqs = {
        // tinit check message
        CreateMuxFrame(0, mux::Type::RerrOld, 1),
        CreateMuxFrame(1, mux::Type::Tdispatch, 1),
        CreateMuxFrame(3, mux::Type::Tdrain, 1),
    };
    std::deque<mux::Frame> resps = {
        // tinit check message response
        CreateMuxFrame(2, mux::Type::Rdispatch, 1),
        CreateMuxFrame(4, mux::Type::Rdrain, 1),
    };
    NoState state;

    RecordsWithErrorCount<mux::Record> res = mux::StitchFrames(&reqs, &resps, &state);

    EXPECT_EQ(res.error_count, 1);
    EXPECT_EQ(res.records.size(), 3);
    EXPECT_EQ(mux::Type(res.records[0].req.type), mux::Type::RerrOld);
    EXPECT_EQ(res.records[0].resp.type, 0);

    EXPECT_THAT(reqs, IsEmpty());
    EXPECT_THAT(resps, IsEmpty());
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
