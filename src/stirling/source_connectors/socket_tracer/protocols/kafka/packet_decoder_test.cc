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

#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/packet_decoder.h"
#include <utility>
#include <vector>
#include "src/common/base/types.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

template <typename T>
struct PacketDecoderTestCase {
  std::string input;
  T expected_output;
};

template <typename T>
class PacketDecoderTest : public ::testing::TestWithParam<PacketDecoderTestCase<T>> {};

class PacketDecoderTestUnsignedVarint : public PacketDecoderTest<int32_t> {};

TEST_P(PacketDecoderTestUnsignedVarint, ExtractUnsignedVarint) {
  PacketDecoder decoder = PacketDecoder(GetParam().input);
  EXPECT_OK_AND_EQ(decoder.ExtractUnsignedVarint(), GetParam().expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AllData, PacketDecoderTestUnsignedVarint,
    ::testing::Values(
        PacketDecoderTestCase<int32_t>{std::string("\x00", 1), 0},
        PacketDecoderTestCase<int32_t>{std::string("\x03", 1), 3},
        PacketDecoderTestCase<int32_t>{std::string("\x96\x01", 2), 150},
        PacketDecoderTestCase<int32_t>{std::string("\xff\xff\xff\xff\x0f", 5), -1},
        PacketDecoderTestCase<int32_t>{std::string("\x80\xC0\xFF\xFF\x0F", 5), -8192},
        PacketDecoderTestCase<int32_t>{std::string("\xff\xff\xff\xff\x07", 5), INT_MAX},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x80\x80\x08", 5), INT_MIN}));

class PacketDecoderTestVarint : public PacketDecoderTest<int32_t> {};

TEST_P(PacketDecoderTestVarint, ExtractVarint) {
  PacketDecoder decoder = PacketDecoder(GetParam().input);
  EXPECT_OK_AND_EQ(decoder.ExtractVarint(), GetParam().expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AllData, PacketDecoderTestVarint,
    ::testing::Values(
        PacketDecoderTestCase<int32_t>{std::string("\x00", 1), 0},
        PacketDecoderTestCase<int32_t>{std::string("\x01", 1), -1},
        PacketDecoderTestCase<int32_t>{std::string("\x02", 1), 1},
        PacketDecoderTestCase<int32_t>{std::string("\x7E", 1), 63},
        PacketDecoderTestCase<int32_t>{std::string("\x7F", 1), -64},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x01", 2), 64},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x01", 2), -65},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\x7F", 2), 8191},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\x7F", 2), -8192},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x01", 3), 8192},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x80\x01", 3), -8193},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\xFF\x7F", 3), 1048575},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\xFF\x7F", 3), -1048576},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x80\x01", 4), 1048576},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x80\x80\x01", 4), -1048577},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\xFF\xFF\x7F", 4), 134217727},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\xFF\xFF\x7F", 4), -134217728},
        PacketDecoderTestCase<int32_t>{std::string("\x80\x80\x80\x80\x01", 5), 134217728},
        PacketDecoderTestCase<int32_t>{std::string("\x81\x80\x80\x80\x01", 5), -134217729},
        PacketDecoderTestCase<int32_t>{std::string("\xFE\xFF\xFF\xFF\x0F", 5), INT_MAX},
        PacketDecoderTestCase<int32_t>{std::string("\xFF\xFF\xFF\xFF\x0F", 5), INT_MIN}));

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
