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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/testing.h"

#include "src/common/base/types.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/decode.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"

namespace px {
namespace stirling {
namespace protocols {
namespace amqp {

class DecodeAllTests
    : public ::testing::TestWithParam<std::tuple<std::string, std::string_view, std::string>> {};

TEST_P(DecodeAllTests, CheckDecoding) {
  std::basic_string_view<char> string_input = std::get<1>(GetParam());
  Frame packet;
  ParseFrame(message_type_t::kRequest, &string_input, &packet);
  std::string expected = std::get<2>(GetParam());
  EXPECT_EQ(packet.ToString(), expected);
}

INSTANTIATE_TEST_SUITE_P(
    DecodeAllTestsSuite, DecodeAllTests,
    ::testing::Values(
        std::make_tuple("hearbeat_test", CreateStringView<char>("\x08\x00\x00\x00\x00\x00\x00\xce"),
                        "frame_type=[8] channel=[0] payload_size=[0] msg=[]"),

        std::make_tuple("content_body",
                        CreateStringView<char>("\x03\x00\x01\x00\x00\x00\x0a\x42\x70\x4c"
                                               "\x6e\x66\x67\x44\x73\x63\x32\xce"),
                        "frame_type=[3] channel=[1] payload_size=[10] msg=[]"),

        std::make_tuple(
            "content_header_basic",
            CreateStringView<char>("\x02\x00\x01\x00\x00\x00\x19\x00\x3c\x00\x00\x00\x00"
                                   "\x00\x00\x00\x00\x00\x0a\x80\x00\x0a\x74\x65\x78\x74"
                                   "\x2f\x70\x6c\x61\x69\x6e\xce"),
            "frame_type=[2] channel=[1] payload_size=[25] "
            "msg=[{\"body_size\":10,\"property_flags\":32768,\"content_type\":"
            "\"text/"
            "plain\",\"content_encoding\":\"\",\"delivery_mode\":0,"
            "\"priority\":0,\"correlation_id\":\"\",\"reply_to\":\"\","
            "\"expiration\":\"\",\"message_id\":\"\",\"timestamp\":0,\"type\":"
            "\"\",\"user_id\":\"\",\"app_id\":\"\",\"reserved\":\"\"}]"),

        std::make_tuple(
            "amqp_method_connection_start",
            CreateStringView<char>("\x01\x00\x00\x00\x00\x02\x00\x00\x0a\x00\x0a\x00\x09\x00\x00"
                                   "\x01\xdb\x0c\x63\x61\x70\x61\x62\x69\x6c\x69\x74\x69\x65\x73"
                                   "\x46\x00\x00\x00\xc7\x12\x70\x75\x62\x6c\x69\x73\x68\x65\x72"
                                   "\x5f\x63\x6f\x6e\x66\x69\x72\x6d\x73\x74\x01\x1a\x65\x78\x63"
                                   "\x68\x61\x6e\x67\x65\x5f\x65\x78\x63\x68\x61\x6e\x67\x65\x5f"
                                   "\x62\x69\x6e\x64\x69\x6e\x67\x73\x74\x01\x0a\x62\x61\x73\x69"
                                   "\x63\x2e\x6e\x61\x63\x6b\x74\x01\x16\x63\x6f\x6e\x73\x75\x6d"
                                   "\x65\x72\x5f\x63\x61\x6e\x63\x65\x6c\x5f\x6e\x6f\x74\x69\x66"
                                   "\x79\x74\x01\x12\x63\x6f\x6e\x6e\x65\x63\x74\x69\x6f\x6e\x2e"
                                   "\x62\x6c\x6f\x63\x6b\x65\x64\x74\x01\x13\x63\x6f\x6e\x73\x75"
                                   "\x6d\x65\x72\x5f\x70\x72\x69\x6f\x72\x69\x74\x69\x65\x73\x74"
                                   "\x01\x1c\x61\x75\x74\x68\x65\x6e\x74\x69\x63\x61\x74\x69\x6f"
                                   "\x6e\x5f\x66\x61\x69\x6c\x75\x72\x65\x5f\x63\x6c\x6f\x73\x65"
                                   "\x74\x01\x10\x70\x65\x72\x5f\x63\x6f\x6e\x73\x75\x6d\x65\x72"
                                   "\x5f\x71\x6f\x73\x74\x01\x0f\x64\x69\x72\x65\x63\x74\x5f\x72"
                                   "\x65\x70\x6c\x79\x5f\x74\x6f\x74\x01\x0c\x63\x6c\x75\x73\x74"
                                   "\x65\x72\x5f\x6e\x61\x6d\x65\x53\x00\x00\x00\x19\x72\x61\x62"
                                   "\x62\x69\x74\x40\x62\x6f\x6d\x62\x65\x2e\x70\x69\x78\x69\x65"
                                   "\x6c\x61\x62\x73\x2e\x61\x69\x09\x63\x6f\x70\x79\x72\x69\x67"
                                   "\x68\x74\x53\x00\x00\x00\x37\x43\x6f\x70\x79\x72\x69\x67\x68"
                                   "\x74\x20\x28\x63\x29\x20\x32\x30\x30\x37\x2d\x32\x30\x32\x32"
                                   "\x20\x56\x4d\x77\x61\x72\x65\x2c\x20\x49\x6e\x63\x2e\x20\x6f"
                                   "\x72\x20\x69\x74\x73\x20\x61\x66\x66\x69\x6c\x69\x61\x74\x65"
                                   "\x73\x2e\x0b\x69\x6e\x66\x6f\x72\x6d\x61\x74\x69\x6f\x6e\x53"
                                   "\x00\x00\x00\x39\x4c\x69\x63\x65\x6e\x73\x65\x64\x20\x75\x6e"
                                   "\x64\x65\x72\x20\x74\x68\x65\x20\x4d\x50\x4c\x20\x32\x2e\x30"
                                   "\x2e\x20\x57\x65\x62\x73\x69\x74\x65\x3a\x20\x68\x74\x74\x70"
                                   "\x73\x3a\x2f\x2f\x72\x61\x62\x62\x69\x74\x6d\x71\x2e\x63\x6f"
                                   "\x6d\x08\x70\x6c\x61\x74\x66\x6f\x72\x6d\x53\x00\x00\x00\x11"
                                   "\x45\x72\x6c\x61\x6e\x67\x2f\x4f\x54\x50\x20\x32\x34\x2e\x32"
                                   "\x2e\x31\x07\x70\x72\x6f\x64\x75\x63\x74\x53\x00\x00\x00\x08"
                                   "\x52\x61\x62\x62\x69\x74\x4d\x51\x07\x76\x65\x72\x73\x69\x6f"
                                   "\x6e\x53\x00\x00\x00\x06\x33\x2e\x39\x2e\x31\x33\x00\x00\x00"
                                   "\x0e\x50\x4c\x41\x49\x4e\x20\x41\x4d\x51\x50\x4c\x41\x49\x4e"
                                   "\x00\x00\x00\x05\x65\x6e\x5f\x55\x53\xce"),
            "frame_type=[1] channel=[0] payload_size=[512] "
            "msg=[{\"version_major\":0,\"version_minor\":9,\"mechanisms\":\"PLAIN "
            "AMQPLAIN\",\"locales\":\"en_US\"}]"),

        std::make_tuple(
            "amqp_method_connection_start_ok",
            CreateStringView<char>("\x01\x00\x00\x00\x00\x00\xa1\x00\x0a\x00\x0b\x00\x00\x00\x7d"
                                   "\x07\x70\x72\x6f\x64\x75\x63\x74\x53\x00\x00\x00\x21\x68\x74"
                                   "\x74\x70\x73\x3a\x2f\x2f\x67\x69\x74\x68\x75\x62\x2e\x63\x6f"
                                   "\x6d\x2f\x73\x74\x72\x65\x61\x64\x77\x61\x79\x2f\x61\x6d\x71"
                                   "\x70\x07\x76\x65\x72\x73\x69\x6f\x6e\x53\x00\x00\x00\x02\xce"
                                   "\xb2\x0c\x63\x61\x70\x61\x62\x69\x6c\x69\x74\x69\x65\x73\x46"
                                   "\x00\x00\x00\x2e\x12\x63\x6f\x6e\x6e\x65\x63\x74\x69\x6f\x6e"
                                   "\x2e\x62\x6c\x6f\x63\x6b\x65\x64\x74\x01\x16\x63\x6f\x6e\x73"
                                   "\x75\x6d\x65\x72\x5f\x63\x61\x6e\x63\x65\x6c\x5f\x6e\x6f\x74"
                                   "\x69\x66\x79\x74\x01\x05\x50\x4c\x41\x49\x4e\x00\x00\x00\x0c"
                                   "\x00\x67\x75\x65\x73\x74\x00\x67\x75\x65\x73\x74\x05\x65\x6e"
                                   "\x5f\x55\x53\xce"),
            "frame_type=[1] channel=[0] payload_size=[161] "
            "msg=[{\"mechanism\":\"PLAIN\",\"response\":\"\\u0000guest\\u0000guest\","
            "\"locale\":\"en_US\"}]"),

        std::make_tuple("amqp_method_connection_tune",
                        CreateStringView<char>("\x01\x00\x00\x00\x00\x00\x0c\x00\x0a\x00"
                                               "\x1e\x07\xff\x00\x02\x00\x00\x00\x3c\xce"),
                        "frame_type=[1] channel=[0] payload_size=[12] "
                        "msg=[{\"channel_max\":2047,\"frame_max\":131072,"
                        "\"heartbeat\":60}]"),

        std::make_tuple("amqp_method_connection_tune_ok",
                        CreateStringView<char>("\x01\x00\x00\x00\x00\x00\x0c\x00\x0a\x00"
                                               "\x1f\x07\xff\x00\x02\x00\x00\x00\x0a\xce"),
                        "frame_type=[1] channel=[0] payload_size=[12] "
                        "msg=[{\"channel_max\":2047,\"frame_max\":131072,"
                        "\"heartbeat\":10}]"),

        std::make_tuple("amqp_method_connection_open",
                        CreateStringView<char>("\x01\x00\x00\x00\x00\x00\x08\x00\x0a\x00"
                                               "\x28\x01\x2f\x00\x00\xce"),
                        "frame_type=[1] channel=[0] payload_size=[8] "
                        "msg=[{\"virtual_host\":\"/"
                        "\",\"reserved_1\":\"\",\"reserved_2\":0}]"),

        std::make_tuple(
            "amqp_method_connection_open_ok",
            CreateStringView<char>("\x01\x00\x00\x00\x00\x00\x05\x00\x0a\x00\x29\x00\xce"),
            "frame_type=[1] channel=[0] payload_size=[5] "
            "msg=[{\"reserved_1\":\"\"}]"),

        std::make_tuple(
            "amqp_method_channel_open",
            CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x05\x00\x14\x00\x0a\x00\xce"),
            "frame_type=[1] channel=[1] payload_size=[5] "
            "msg=[{\"reserved_1\":\"\"}]"),

        std::make_tuple("amqp_method_channel_open_ok",
                        CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x08\x00\x14\x00"
                                               "\x0b\x00\x00\x00\x00\xce"),
                        "frame_type=[1] channel=[1] payload_size=[8] "
                        "msg=[{\"reserved_1\":\"\"}]"),

        std::make_tuple(
            "amqp_method_queue_declare",
            CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x11\x00\x32\x00\x0a\x00\x00"
                                   "\x05\x68\x65\x6c\x6c\x6f\x00\x00\x00\x00\x00\xce"),
            "frame_type=[1] channel=[1] payload_size=[17] "
            "msg=[{\"reserved_1\":0,\"queue\":\"hello\",\"passive\":0,"
            "\"durable\":0,\"exclusive\":0,\"auto_delete\":0,\"no_wait\":0}]"),

        std::make_tuple(
            "amqp_method_queue_declare_ok",
            CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x12\x00\x32\x00\x0b\x05\x68"
                                   "\x65\x6c\x6c\x6f\x00\x00\x00\x00\x00\x00\x00\x00\xce"),
            "frame_type=[1] channel=[1] payload_size=[18] "
            "msg=[{\"queue\":\"hello\",\"message_count\":0,\"consumer_count\":"
            "0}]"),

        std::make_tuple(
            "amqp_method_basic_consume",
            CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x3d\x00\x3c\x00\x14\x00\x00\x05\x68"
                                   "\x65\x6c\x6c\x6f\x2b\x63\x74\x61\x67\x2d\x2f\x74\x6d\x70\x2f"
                                   "\x67\x6f\x2d\x62\x75\x69\x6c\x64\x34\x35\x38\x31\x37\x33\x31"
                                   "\x38\x39\x2f\x62\x30\x30\x31\x2f\x65\x78\x65\x2f\x72\x65\x63"
                                   "\x76\x2d\x31\x02\x00\x00\x00\x00\xce"),
            "frame_type=[1] channel=[1] payload_size=[61] "
            "msg=[{\"reserved_1\":0,\"queue\":\"hello\",\"consumer_tag\":"
            "\"ctag-/tmp/go-build458173189/b001/exe/"
            "recv-1\",\"no_local\":0,\"no_ack\":1,\"exclusive\":0,\"no_wait\":"
            "0}]"),

        std::make_tuple(
            "amqp_method_basic_consume_ok",
            CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x30\x00\x3c\x00\x15\x2b\x63\x74\x61"
                                   "\x67\x2d\x2f\x74\x6d\x70\x2f\x67\x6f\x2d\x62\x75\x69\x6c\x64"
                                   "\x34\x35\x38\x31\x37\x33\x31\x38\x39\x2f\x62\x30\x30\x31\x2f"
                                   "\x65\x78\x65\x2f\x72\x65\x63\x76\x2d\x31\xce"),
            "frame_type=[1] channel=[1] payload_size=[48] "
            "msg=[{\"consumer_tag\":\"ctag-/tmp/go-build458173189/b001/exe/"
            "recv-1\"}]"),

        std::make_tuple(
            "amqp_method_basic_publish",
            CreateStringView<char>("\x01\x00\x01\x00\x00\x00\x0f\x00\x3c\x00\x28\x00\x00"
                                   "\x00\x06\x68\x65\x6c\x6c\x6f\x32\x00\xce"),
            "frame_type=[1] channel=[1] payload_size=[15] "
            "msg=[{\"reserved_1\":0,\"exchange\":\""
            "\",\"routing_key\":\"hello2\",\"mandatory\":0,\"immediate\":0}]"),

        std::make_tuple(
            "amqp_method_basic_deliver",
            CreateStringView<char>("\x01\x00\x02\x00\x00\x00\x41\x00\x3c\x00\x3c\x2b\x63\x74\x61"
                                   "\x67\x2d\x2f\x74\x6d\x70\x2f\x67\x6f\x2d\x62\x75\x69\x6c\x64"
                                   "\x34\x35\x38\x31\x37\x33\x31\x38\x39\x2f\x62\x30\x30\x31\x2f"
                                   "\x65\x78\x65\x2f\x72\x65\x63\x76\x2d\x32\x00\x00\x00\x00\x00"
                                   "\x00\x00\x01\x00\x00\x06\x68\x65\x6c\x6c\x6f\x32\xce"),
            "frame_type=[1] channel=[2] payload_size=[65] "
            "msg=[{\"consumer_tag\":\"ctag-/tmp/go-build458173189/b001/exe/"
            "recv-2\",\"delivery_tag\":1,\"redelivered\":0,\"exchange\":"
            "\"\",\"routing_key\":\"hello2\"}]")),

    [](const ::testing::TestParamInfo<DecodeAllTests::ParamType>& info) {
      std::string title = std::get<0>(info.param);
      return title;
    });

}  // namespace amqp
}  // namespace protocols
}  // namespace stirling
}  // namespace px
