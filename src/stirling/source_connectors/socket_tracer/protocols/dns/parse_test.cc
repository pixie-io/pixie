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

#include <absl/container/flat_hash_map.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/dns/parse.h"

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

// The test data below was captured via WireShark.
// Process involved triggering a DNS request with `dig` or `nslookup`.

// Domain Name System (query)
// Transaction ID: 0xc6fa
// Flags: 0x0100 Standard query
// Questions: 1
// Answer RRs: 0
// Authority RRs: 0
// Additional RRs: 1
// Queries
//         intellij-experiments.appspot.com: type A, class IN
// Additional records
constexpr uint8_t kQueryFrame[] = {
    0xc6, 0xfa, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x14, 0x69, 0x6e, 0x74,
    0x65, 0x6c, 0x6c, 0x69, 0x6a, 0x2d, 0x65, 0x78, 0x70, 0x65, 0x72, 0x69, 0x6d, 0x65, 0x6e, 0x74,
    0x73, 0x07, 0x61, 0x70, 0x70, 0x73, 0x70, 0x6f, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// The corresponding response to the query above.
//   Domain Name System (response)
//   Transaction ID: 0xc6fa
//   Flags: 0x8180 Standard query response, No error
//   Questions: 1
//   Answer RRs: 1
//   Authority RRs: 0
//   Additional RRs: 1
//   Queries
//           intellij-experiments.appspot.com: type A, class IN
//   Answers
//           intellij-experiments.appspot.com: type A, class IN, addr 216.58.194.180
//   Additional records
constexpr uint8_t kRespFrame[] = {
    0xc6, 0xfa, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x14, 0x69, 0x6e, 0x74,
    0x65, 0x6c, 0x6c, 0x69, 0x6a, 0x2d, 0x65, 0x78, 0x70, 0x65, 0x72, 0x69, 0x6d, 0x65, 0x6e, 0x74,
    0x73, 0x07, 0x61, 0x70, 0x70, 0x73, 0x70, 0x6f, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01,
    0x00, 0x01, 0xc0, 0x0c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x24, 0x00, 0x04, 0xd8, 0x3a,
    0xc2, 0xb4, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// Domain Name System (query)
// Transaction ID: 0xfeae
// Flags: 0x0100 Standard query
// 0... .... .... .... = Response: Message is a query
// .000 0... .... .... = Opcode: Standard query (0)
// .... ..0. .... .... = Truncated: Message is not truncated
// .... ...1 .... .... = Recursion desired: Do query recursively
// .... .... .0.. .... = Z: reserved (0)
// .... .... ...0 .... = Non-authenticated data: Unacceptable
//         Questions: 1
// Answer RRs: 0
// Authority RRs: 0
// Additional RRs: 0
// Queries
//         www.yahoo.com: type A, class IN
//         Name: www.yahoo.com
// [Name Length: 13]
// [Label Count: 3]
// Type: A (Host Address) (1)
// Class: IN (0x0001)
constexpr uint8_t kReqFrame2[] = {0xfe, 0xae, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x03, 0x77, 0x77, 0x77, 0x05, 0x79, 0x61, 0x68, 0x6f, 0x6f,
                                  0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01};

// Domain Name System (response)
// Transaction ID: 0xfeae
// Flags: 0x8180 Standard query response, No error
// 1... .... .... .... = Response: Message is a response
// .000 0... .... .... = Opcode: Standard query (0)
// .... .0.. .... .... = Authoritative: Server is not an authority for domain
// .... ..0. .... .... = Truncated: Message is not truncated
// .... ...1 .... .... = Recursion desired: Do query recursively
// .... .... 1... .... = Recursion available: Server can do recursive queries
// .... .... .0.. .... = Z: reserved (0)
// .... .... ..0. .... = Answer authenticated: Answer/authority portion was not authenticated by the
// server
// .... .... ...0 .... = Non-authenticated data: Unacceptable
// .... .... .... 0000 = Reply code: No error (0)
// Questions: 1
// Answer RRs: 5
// Authority RRs: 0
// Additional RRs: 0
// Queries
//         www.yahoo.com: type A, class IN
//         Name: www.yahoo.com
// [Name Length: 13]
// [Label Count: 3]
// Type: A (Host Address) (1)
// Class: IN (0x0001)
// Answers
//         www.yahoo.com: type CNAME, class IN, cname new-fp-shed.wg1.b.yahoo.com
//         Name: www.yahoo.com
//         Type: CNAME (Canonical NAME for an alias) (5)
// Class: IN (0x0001)
// Time to live: 57 (57 seconds)
// Data length: 20
// CNAME: new-fp-shed.wg1.b.yahoo.com
// new-fp-shed.wg1.b.yahoo.com: type A, class IN, addr 98.137.11.164
// Name: new-fp-shed.wg1.b.yahoo.com
//         Type: A (Host Address) (1)
// Class: IN (0x0001)
// Time to live: 57 (57 seconds)
// Data length: 4
// Address: 98.137.11.164
// new-fp-shed.wg1.b.yahoo.com: type A, class IN, addr 74.6.231.20
// Name: new-fp-shed.wg1.b.yahoo.com
//         Type: A (Host Address) (1)
// Class: IN (0x0001)
// Time to live: 57 (57 seconds)
// Data length: 4
// Address: 74.6.231.20
// new-fp-shed.wg1.b.yahoo.com: type A, class IN, addr 74.6.231.21
// Name: new-fp-shed.wg1.b.yahoo.com
//         Type: A (Host Address) (1)
// Class: IN (0x0001)
// Time to live: 57 (57 seconds)
// Data length: 4
// Address: 74.6.231.21
// new-fp-shed.wg1.b.yahoo.com: type A, class IN, addr 98.137.11.163
// Name: new-fp-shed.wg1.b.yahoo.com
//         Type: A (Host Address) (1)
// Class: IN (0x0001)
// Time to live: 57 (57 seconds)
// Data length: 4
// Address: 98.137.11.163
constexpr uint8_t kRespFrame2[] = {
    0xfe, 0xae, 0x81, 0x80, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x03, 0x77, 0x77, 0x77,
    0x05, 0x79, 0x61, 0x68, 0x6f, 0x6f, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0,
    0x0c, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x39, 0x00, 0x14, 0x0b, 0x6e, 0x65, 0x77, 0x2d,
    0x66, 0x70, 0x2d, 0x73, 0x68, 0x65, 0x64, 0x03, 0x77, 0x67, 0x31, 0x01, 0x62, 0xc0, 0x10, 0xc0,
    0x2b, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x39, 0x00, 0x04, 0x62, 0x89, 0x0b, 0xa4, 0xc0,
    0x2b, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x39, 0x00, 0x04, 0x4a, 0x06, 0xe7, 0x14, 0xc0,
    0x2b, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x39, 0x00, 0x04, 0x4a, 0x06, 0xe7, 0x15, 0xc0,
    0x2b, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x39, 0x00, 0x04, 0x62, 0x89, 0x0b, 0xa3};

// Domain Name System (response)
//    Transaction ID: 0x938f
//    Flags: 0x8180 Standard query response, No error
//        1... .... .... .... = Response: Message is a response
//        .000 0... .... .... = Opcode: Standard query (0)
//        .... .0.. .... .... = Authoritative: Server is not an authority for domain
//        .... ..0. .... .... = Truncated: Message is not truncated
//        .... ...1 .... .... = Recursion desired: Do query recursively
//        .... .... 1... .... = Recursion available: Server can do recursive queries
//        .... .... .0.. .... = Z: reserved (0)
//        .... .... ..0. .... = Answer authenticated: Answer/authority portion was not authenticated
//        by the server
//        .... .... ...0 .... = Non-authenticated data: Unacceptable
//        .... .... .... 0000 = Reply code: No error (0)
//    Questions: 1
//    Answer RRs: 5
//    Authority RRs: 0
//    Additional RRs: 1
//    Queries
//        www.reddit.com: type A, class IN
//            Name: www.reddit.com
//            [Name Length: 14]
//            [Label Count: 3]
//            Type: A (Host Address) (1)
//            Class: IN (0x0001)
//    Answers
//        www.reddit.com: type CNAME, class IN, cname reddit.map.fastly.net
//            Name: www.reddit.com
//            Type: CNAME (Canonical NAME for an alias) (5)
//            Class: IN (0x0001)
//            Time to live: 190 (3 minutes, 10 seconds)
//            Data length: 23
//            CNAME: reddit.map.fastly.net
//        reddit.map.fastly.net: type A, class IN, addr 151.101.1.140
//            Name: reddit.map.fastly.net
//            Type: A (Host Address) (1)
//            Class: IN (0x0001)
//            Time to live: 29 (29 seconds)
//            Data length: 4
//            Address: 151.101.1.140
//        reddit.map.fastly.net: type A, class IN, addr 151.101.65.140
//            Name: reddit.map.fastly.net
//            Type: A (Host Address) (1)
//            Class: IN (0x0001)
//            Time to live: 29 (29 seconds)
//            Data length: 4
//            Address: 151.101.65.140
//        reddit.map.fastly.net: type A, class IN, addr 151.101.129.140
//            Name: reddit.map.fastly.net
//            Type: A (Host Address) (1)
//            Class: IN (0x0001)
//            Time to live: 29 (29 seconds)
//            Data length: 4
//            Address: 151.101.129.140
//        reddit.map.fastly.net: type A, class IN, addr 151.101.193.140
//            Name: reddit.map.fastly.net
//            Type: A (Host Address) (1)
//            Class: IN (0x0001)
//            Time to live: 29 (29 seconds)
//            Data length: 4
//            Address: 151.101.193.140
//    Additional records
//        <Root>: type OPT
//            Name: <Root>
//            Type: OPT (41)
//            UDP payload size: 512
//            Higher bits in extended RCODE: 0x00
//            EDNS0 version: 0
//            Z: 0x0000
//                0... .... .... .... = DO bit: Cannot handle DNSSEC security RRs
//                .000 0000 0000 0000 = Reserved: 0x0000
//            Data length: 0
//    [Request In: 131]
//    [Time: 0.027542535 seconds]
constexpr uint8_t kRespFrame3[] = {
    0x93, 0x8f, 0x81, 0x80, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x03, 0x77, 0x77, 0x77,
    0x06, 0x72, 0x65, 0x64, 0x64, 0x69, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01,
    0xc0, 0x0c, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xbe, 0x00, 0x17, 0x06, 0x72, 0x65, 0x64,
    0x64, 0x69, 0x74, 0x03, 0x6d, 0x61, 0x70, 0x06, 0x66, 0x61, 0x73, 0x74, 0x6c, 0x79, 0x03, 0x6e,
    0x65, 0x74, 0x00, 0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x04, 0x97,
    0x65, 0x01, 0x8c, 0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x04, 0x97,
    0x65, 0x41, 0x8c, 0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x04, 0x97,
    0x65, 0x81, 0x8c, 0xc0, 0x2c, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x04, 0x97,
    0x65, 0xc1, 0x8c, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

class DNSParserTest : public ::testing::Test {};

TEST_F(DNSParserTest, BasicReq) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kQueryFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);

  stream_id_t only_key =
      frames.begin()->first;  // Grab the first (and only) key. DNS has no notion of streams.
  ASSERT_EQ(frames[only_key].size(), 1);
  Frame& first_frame = frames[only_key][0];

  EXPECT_EQ(first_frame.header.txid, 0xc6fa);
  EXPECT_EQ(first_frame.header.flags, 0x0100);
  EXPECT_EQ(first_frame.header.num_queries, 1);
  EXPECT_EQ(first_frame.header.num_answers, 0);
  EXPECT_EQ(first_frame.header.num_auth, 0);
  EXPECT_EQ(first_frame.header.num_addl, 1);

  ASSERT_EQ(first_frame.records().size(), 1);
  EXPECT_EQ(first_frame.records()[0].name, "intellij-experiments.appspot.com");
  EXPECT_EQ(first_frame.records()[0].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[0].addr.AddrStr(), "0.0.0.0");
}

TEST_F(DNSParserTest, BasicResp) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);

  stream_id_t only_key =
      frames.begin()->first;  // Grab the first (and only) key. DNS has no notion of streams.
  ASSERT_EQ(frames[only_key].size(), 1);
  Frame& first_frame = frames[only_key][0];

  EXPECT_EQ(first_frame.header.txid, 0xc6fa);
  EXPECT_EQ(first_frame.header.flags, 0x8180);
  EXPECT_EQ(first_frame.header.num_queries, 1);
  EXPECT_EQ(first_frame.header.num_answers, 1);
  EXPECT_EQ(first_frame.header.num_auth, 0);
  EXPECT_EQ(first_frame.header.num_addl, 1);

  ASSERT_EQ(first_frame.records().size(), 1);
  EXPECT_EQ(first_frame.records()[0].name, "intellij-experiments.appspot.com");
  EXPECT_EQ(first_frame.records()[0].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[0].addr.AddrStr(), "216.58.194.180");
}

TEST_F(DNSParserTest, BasicReq2) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kReqFrame2));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);

  stream_id_t only_key =
      frames.begin()->first;  // Grab the first (and only) key. DNS has no notion of streams.
  ASSERT_EQ(frames[only_key].size(), 1);
  Frame& first_frame = frames[only_key][0];

  EXPECT_EQ(first_frame.header.txid, 0xfeae);
  EXPECT_EQ(first_frame.header.flags, 0x0100);
  EXPECT_EQ(first_frame.header.num_queries, 1);
  EXPECT_EQ(first_frame.header.num_answers, 0);
  EXPECT_EQ(first_frame.header.num_auth, 0);
  EXPECT_EQ(first_frame.header.num_addl, 0);

  ASSERT_EQ(first_frame.records().size(), 1);
  EXPECT_EQ(first_frame.records()[0].name, "www.yahoo.com");
  EXPECT_EQ(first_frame.records()[0].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[0].addr.AddrStr(), "0.0.0.0");
}

TEST_F(DNSParserTest, CNameAndMultipleResponses) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame2));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);
  stream_id_t only_key =
      frames.begin()->first;  // Grab the first (and only) key. DNS has no notion of streams.
  ASSERT_EQ(frames[only_key].size(), 1);
  Frame& first_frame = frames[only_key][0];

  EXPECT_EQ(first_frame.header.txid, 0xfeae);
  EXPECT_EQ(first_frame.header.flags, 0x8180);
  EXPECT_EQ(first_frame.header.num_queries, 1);
  EXPECT_EQ(first_frame.header.num_answers, 5);
  EXPECT_EQ(first_frame.header.num_auth, 0);
  EXPECT_EQ(first_frame.header.num_addl, 0);

  ASSERT_EQ(first_frame.records().size(), 5);

  EXPECT_EQ(first_frame.records()[0].name, "www.yahoo.com");
  EXPECT_EQ(first_frame.records()[0].addr.family, InetAddrFamily::kUnspecified);
  EXPECT_EQ(first_frame.records()[0].cname, "new-fp-shed.wg1.b.yahoo.com");

  EXPECT_EQ(first_frame.records()[1].name, "new-fp-shed.wg1.b.yahoo.com");
  EXPECT_EQ(first_frame.records()[1].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[1].addr.AddrStr(), "98.137.11.164");
  EXPECT_EQ(first_frame.records()[1].cname, "");

  EXPECT_EQ(first_frame.records()[2].name, "new-fp-shed.wg1.b.yahoo.com");
  EXPECT_EQ(first_frame.records()[2].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[2].addr.AddrStr(), "74.6.231.20");
  EXPECT_EQ(first_frame.records()[2].cname, "");

  EXPECT_EQ(first_frame.records()[3].name, "new-fp-shed.wg1.b.yahoo.com");
  EXPECT_EQ(first_frame.records()[3].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[3].addr.AddrStr(), "74.6.231.21");
  EXPECT_EQ(first_frame.records()[3].cname, "");

  EXPECT_EQ(first_frame.records()[4].name, "new-fp-shed.wg1.b.yahoo.com");
  EXPECT_EQ(first_frame.records()[4].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[4].addr.AddrStr(), "98.137.11.163");
  EXPECT_EQ(first_frame.records()[4].cname, "");
}

TEST_F(DNSParserTest, CNameAndMultipleResponses2) {
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame3));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);

  stream_id_t only_key =
      frames.begin()->first;  // Grab the first (and only) key. DNS has no notion of streams.
  ASSERT_EQ(frames[only_key].size(), 1);
  Frame& first_frame = frames[only_key][0];

  EXPECT_EQ(first_frame.header.txid, 0x938f);
  EXPECT_EQ(first_frame.header.flags, 0x8180);
  EXPECT_EQ(first_frame.header.num_queries, 1);
  EXPECT_EQ(first_frame.header.num_answers, 5);
  EXPECT_EQ(first_frame.header.num_auth, 0);
  EXPECT_EQ(first_frame.header.num_addl, 1);
  ASSERT_EQ(first_frame.records().size(), 5);

  EXPECT_EQ(first_frame.records()[0].name, "www.reddit.com");
  EXPECT_EQ(first_frame.records()[0].addr.family, InetAddrFamily::kUnspecified);
  EXPECT_EQ(first_frame.records()[0].cname, "reddit.map.fastly.net");

  EXPECT_EQ(first_frame.records()[1].name, "reddit.map.fastly.net");
  EXPECT_EQ(first_frame.records()[1].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[1].addr.AddrStr(), "151.101.1.140");
  EXPECT_EQ(first_frame.records()[1].cname, "");

  EXPECT_EQ(first_frame.records()[2].name, "reddit.map.fastly.net");
  EXPECT_EQ(first_frame.records()[2].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[2].addr.AddrStr(), "151.101.65.140");
  EXPECT_EQ(first_frame.records()[2].cname, "");

  EXPECT_EQ(first_frame.records()[3].name, "reddit.map.fastly.net");
  EXPECT_EQ(first_frame.records()[3].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[3].addr.AddrStr(), "151.101.129.140");
  EXPECT_EQ(first_frame.records()[3].cname, "");

  EXPECT_EQ(first_frame.records()[4].name, "reddit.map.fastly.net");
  EXPECT_EQ(first_frame.records()[4].addr.family, InetAddrFamily::kIPv4);
  EXPECT_EQ(first_frame.records()[4].addr.AddrStr(), "151.101.193.140");
  EXPECT_EQ(first_frame.records()[4].cname, "");
}

TEST_F(DNSParserTest, IncompleteHeader) {
  constexpr uint8_t kIncompleteHeader[] = {0xc6, 0xfa, 0x01, 0x00, 0x00, 0x01,
                                           0x00, 0x00, 0x00, 0x00, 0x00};
  auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kIncompleteHeader));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kInvalid);
}

// NOTE that some partial records parse correctly, while others don't.
// Should modify the submodule so that all are reported as invalid.
TEST_F(DNSParserTest, PartialRecords) {
  {
    auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame));
    frame_view.remove_suffix(10);

    absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
    ParseResult<stream_id_t> parse_result =
        ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

    ASSERT_EQ(parse_result.state, ParseState::kSuccess);
  }

  {
    auto frame_view = CreateStringView<char>(CharArrayStringView<uint8_t>(kRespFrame));
    frame_view.remove_suffix(20);

    absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
    ParseResult<stream_id_t> parse_result =
        ParseFramesLoop(message_type_t::kRequest, frame_view, &frames);

    ASSERT_EQ(parse_result.state, ParseState::kInvalid);
  }
}

// Regression test: RFC 1035 s4.1.4 compression pointers are 2 bytes with a 14-bit offset.
// The bug was in dnsReadName: when it encountered a compression pointer inside a name, it
// only read the low byte of the 2-byte pointer, discarding the upper 6 bits of the offset.
// This test exercises that code path by constructing a valid DNS response where dnsReadName
// encounters a chained compression pointer whose offset is > 255.
//
// The critical path: answer 19 is a CNAME whose rdata name is "cdn" followed by a compression
// pointer to "example.com" label-encoded at offset > 255. When dnsReadName parses this CNAME
// rdata, it reads "cdn", then hits the compression pointer and recurses — this is the code
// path where the old 1-byte offset read would compute the wrong offset.
TEST_F(DNSParserTest, CompressionPointerOffset14Bit) {
  std::vector<uint8_t> pkt;

  // --- DNS Header (12 bytes) ---
  pkt.push_back(0xAB); pkt.push_back(0xCD);  // txid
  pkt.push_back(0x81); pkt.push_back(0x80);  // flags: standard response
  pkt.push_back(0x00); pkt.push_back(0x01);  // 1 query
  pkt.push_back(0x00); pkt.push_back(0x14);  // 20 answers
  pkt.push_back(0x00); pkt.push_back(0x00);  // 0 authority
  pkt.push_back(0x00); pkt.push_back(0x00);  // 0 additional

  // --- Query section (offset 12) ---
  // Name: "www.example.com"
  ASSERT_EQ(pkt.size(), 12u);
  pkt.push_back(0x03); pkt.insert(pkt.end(), {'w','w','w'});
  pkt.push_back(0x07); pkt.insert(pkt.end(), {'e','x','a','m','p','l','e'});
  pkt.push_back(0x03); pkt.insert(pkt.end(), {'c','o','m'});
  pkt.push_back(0x00);
  pkt.push_back(0x00); pkt.push_back(0x01);  // type A
  pkt.push_back(0x00); pkt.push_back(0x01);  // class IN
  // Query ends at offset 33.

  // --- Answers 1-18: A records (18 * 16 = 288 bytes, offsets 33-320) ---
  for (int i = 0; i < 18; i++) {
    pkt.push_back(0xC0); pkt.push_back(0x0C);  // name: ptr to offset 12
    pkt.push_back(0x00); pkt.push_back(0x01);  // type A
    pkt.push_back(0x00); pkt.push_back(0x01);  // class IN
    pkt.push_back(0x00); pkt.push_back(0x00);
    pkt.push_back(0x00); pkt.push_back(0x3C);  // TTL=60
    pkt.push_back(0x00); pkt.push_back(0x04);  // rdlength=4
    pkt.push_back(10); pkt.push_back(0);
    pkt.push_back(0); pkt.push_back(static_cast<uint8_t>(i + 1));
  }
  ASSERT_EQ(pkt.size(), 321u);

  // --- Answer 19 (offset 321): A record with inline name that puts "example.com"
  //     label-encoded at a known offset > 255. ---
  // Name: "other.example.com" written inline so "example.com" starts at offset 327.
  // offset 321
  pkt.push_back(0x05); pkt.insert(pkt.end(), {'o','t','h','e','r'});
  // "example" label starts here:
  size_t example_offset = pkt.size();  // 327 = 0x147
  pkt.push_back(0x07); pkt.insert(pkt.end(), {'e','x','a','m','p','l','e'});
  pkt.push_back(0x03); pkt.insert(pkt.end(), {'c','o','m'});
  pkt.push_back(0x00);
  ASSERT_EQ(example_offset, 327u);
  // type A, class IN, TTL, rdlen, addr
  pkt.push_back(0x00); pkt.push_back(0x01);
  pkt.push_back(0x00); pkt.push_back(0x01);
  pkt.push_back(0x00); pkt.push_back(0x00);
  pkt.push_back(0x00); pkt.push_back(0x3C);
  pkt.push_back(0x00); pkt.push_back(0x04);
  pkt.push_back(10); pkt.push_back(0); pkt.push_back(0); pkt.push_back(19);

  // --- Answer 20 (offset ~357): CNAME record whose rdata name contains a chained
  //     compression pointer to offset 327 (0x147) = "example.com".
  //     rdata name: "cdn" + compression pointer 0xC1 0x47 → "cdn.example.com"
  //     This is the code path through dnsReadName that triggers the bug. ---
  pkt.push_back(0xC0); pkt.push_back(0x0C);  // name: ptr to "www.example.com"
  pkt.push_back(0x00); pkt.push_back(0x05);  // type CNAME
  pkt.push_back(0x00); pkt.push_back(0x01);  // class IN
  pkt.push_back(0x00); pkt.push_back(0x00);
  pkt.push_back(0x00); pkt.push_back(0x3C);  // TTL=60
  // rdata: "cdn" (4 bytes) + compression pointer (2 bytes) = 6 bytes
  pkt.push_back(0x00); pkt.push_back(0x06);  // rdlength=6
  // rdata: label "cdn" then pointer to offset 327 (0xC1 0x47)
  pkt.push_back(0x03); pkt.insert(pkt.end(), {'c','d','n'});
  pkt.push_back(0xC1); pkt.push_back(0x47);  // ptr to offset 0x147 = 327

  auto frame_view = CreateStringView<char>(
      std::string_view(reinterpret_cast<const char*>(pkt.data()), pkt.size()));

  absl::flat_hash_map<stream_id_t, std::deque<Frame>> frames;
  ParseResult<stream_id_t> parse_result =
      ParseFramesLoop(message_type_t::kResponse, frame_view, &frames);

  ASSERT_EQ(parse_result.state, ParseState::kSuccess);

  stream_id_t only_key = frames.begin()->first;
  ASSERT_EQ(frames[only_key].size(), 1);
  Frame& frame = frames[only_key][0];

  ASSERT_EQ(frame.records().size(), 20);

  // The critical assertion: answer 20's CNAME rdata was parsed by dnsReadName which
  // encountered "cdn" then a compression pointer 0xC1 0x47 (offset 327). With the old
  // code this would read only 0x47 (71) — wrong offset. With the fix it correctly reads
  // 327 and resolves to "cdn.example.com".
  EXPECT_EQ(frame.records()[19].cname, "cdn.example.com");
}

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
