/*
 * Copyright 2018- The Pixie Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

// This must be the first include.
#include "src/stirling/bpf_tools/bcc_bpf/stubs.h"

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf/protocol_inference.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/common.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.h"

TEST(ProtocolInferenceTest, Postgres) {
  constexpr char kStartupMessage[] =
      "\x00\x00\x00\x54\x00\x03\x00\x00\x75\x73\x65\x72\x00\x70\x6f\x73"
      "\x74\x67\x72\x65\x73\x00\x64\x61\x74\x61\x62\x61\x73\x65\x00\x70"
      "\x6f\x73\x74\x67\x72\x65\x73\x00\x61\x70\x70\x6c\x69\x63\x61\x74"
      "\x69\x6f\x6e\x5f\x6e\x61\x6d\x65\x00\x70\x73\x71\x6c\x00\x63\x6c"
      "\x69\x65\x6e\x74\x5f\x65\x6e\x63\x6f\x64\x69\x6e\x67\x00\x55\x54"
      "\x46\x38\x00\x00";

  EXPECT_EQ(kRequest, infer_pgsql_startup_message(kStartupMessage, sizeof(kStartupMessage)));
  EXPECT_EQ(kRequest, infer_pgsql_message(kStartupMessage, sizeof(kStartupMessage)));

  constexpr char kQueryMessage[] =
      "\x51\x00\x00\x00\x22\x63\x72\x65\x61\x74\x65\x20\x74\x61\x62\x6c"
      "\x65\x20\x66\x6f\x6f\x20\x28\x66\x31\x20\x73\x65\x72\x69\x61\x6c"
      "\x29\x3b\x00";
  EXPECT_EQ(kRequest, infer_pgsql_message(kQueryMessage, sizeof(kQueryMessage)));
}

TEST(ProtocolInferenceTest, DNS) {
  // A query captured via WireShark:
  //   Domain Name System (query)
  //   Transaction ID: 0xc6fa
  //   Flags: 0x0100 Standard query
  //   Questions: 1
  //   Answer RRs: 0
  //   Authority RRs: 0
  //   Additional RRs: 1
  //   Queries
  //           intellij-experiments.appspot.com: type A, class IN
  //   Additional records
  constexpr uint8_t kQueryFrame[] = {
      0xc6, 0xfa, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x14,
      0x69, 0x6e, 0x74, 0x65, 0x6c, 0x6c, 0x69, 0x6a, 0x2d, 0x65, 0x78, 0x70, 0x65,
      0x72, 0x69, 0x6d, 0x65, 0x6e, 0x74, 0x73, 0x07, 0x61, 0x70, 0x70, 0x73, 0x70,
      0x6f, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
      0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
      0xc6, 0xfa, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x14,
      0x69, 0x6e, 0x74, 0x65, 0x6c, 0x6c, 0x69, 0x6a, 0x2d, 0x65, 0x78, 0x70, 0x65,
      0x72, 0x69, 0x6d, 0x65, 0x6e, 0x74, 0x73, 0x07, 0x61, 0x70, 0x70, 0x73, 0x70,
      0x6f, 0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c,
      0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x24, 0x00, 0x04, 0xd8, 0x3a, 0xc2,
      0xb4, 0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Domain Name System (query)
  // Transaction ID: 0xc1bf
  // Flags: 0x0120 Standard query
  //     0... .... .... .... = Response: Message is a query
  //     .000 0... .... .... = Opcode: Standard query (0)
  //     .... ..0. .... .... = Truncated: Message is not truncated
  //     .... ...1 .... .... = Recursion desired: Do query recursively
  //     .... .... .0.. .... = Z: reserved (0)
  //     .... .... ..1. .... = AD bit: Set
  //     .... .... ...0 .... = Non-authenticated data: Unacceptable
  // Questions: 1
  // Answer RRs: 0
  // Authority RRs: 0
  // Additional RRs: 1
  // Queries
  //     server.dnstest.com: type A, class IN
  //         Name: server.dnstest.com
  //         [Name Length: 18]
  //         [Label Count: 3]
  //         Type: A (Host Address) (1)
  //         Class: IN (0x0001)
  // Additional records
  //     <Root>: type OPT
  //         Name: <Root>
  //         Type: OPT (41)
  //         UDP payload size: 4096
  //         Higher bits in extended RCODE: 0x00
  //         EDNS0 version: 0
  //         Z: 0x0000
  //             0... .... .... .... = DO bit: Cannot handle DNSSEC security RRs
  //             .000 0000 0000 0000 = Reserved: 0x0000
  //         Data length: 12
  //         Option: COOKIE
  //             Option Code: COOKIE (10)
  //             Option Length: 8
  //             Option Data: 2d84e010d683af48
  //             Client Cookie: 2d84e010d683af48
  //             Server Cookie: <MISSING>
  constexpr uint8_t kQueryFrame2[] = {
      0xc1, 0xbf, 0x01, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x06, 0x73, 0x65,
      0x72, 0x76, 0x65, 0x72, 0x07, 0x64, 0x6e, 0x73, 0x74, 0x65, 0x73, 0x74, 0x03, 0x63, 0x6f,
      0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x29, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x0c, 0x00, 0x0a, 0x00, 0x08, 0x2d, 0x84, 0xe0, 0x10, 0xd6, 0x83, 0xaf, 0x48};

  // Domain Name System (response)
  // Transaction ID: 0xc1bf
  // Flags: 0x8580 Standard query response, No error
  // 1... .... .... .... = Response: Message is a response
  // .000 0... .... .... = Opcode: Standard query (0)
  // .... .1.. .... .... = Authoritative: Server is an authority for domain
  // .... ..0. .... .... = Truncated: Message is not truncated
  // .... ...1 .... .... = Recursion desired: Do query recursively
  // .... .... 1... .... = Recursion available: Server can do recursive queries
  // .... .... .0.. .... = Z: reserved (0)
  // .... .... ..0. .... = Answer authenticated: Answer/authority portion was not authenticated by
  // the server
  // .... .... ...0 .... = Non-authenticated data: Unacceptable
  // .... .... .... 0000 = Reply code: No error (0)
  // Questions: 1
  // Answer RRs: 1
  // Authority RRs: 0
  // Additional RRs: 1
  // Queries
  //         server.dnstest.com: type A, class IN
  //         Name: server.dnstest.com
  // [Name Length: 18]
  // [Label Count: 3]
  // Type: A (Host Address) (1)
  // Class: IN (0x0001)
  // Answers
  //         server.dnstest.com: type A, class IN, addr 192.168.32.200
  // Name: server.dnstest.com
  //         Type: A (Host Address) (1)
  // Class: IN (0x0001)
  // Time to live: 86400 (1 day)
  // Data length: 4
  // Address: 192.168.32.200
  // Additional records
  //         <Root>: type OPT
  // Name: <Root>
  //         Type: OPT (41)
  // UDP payload size: 4096
  // Higher bits in extended RCODE: 0x00
  // EDNS0 version: 0
  // Z: 0x0000
  // 0... .... .... .... = DO bit: Cannot handle DNSSEC security RRs
  // .000 0000 0000 0000 = Reserved: 0x0000
  // Data length: 28
  // Option: COOKIE
  //         Option Code: COOKIE (10)
  // Option Length: 24
  // Option Data: 2d84e010d683af48010000005f87d5e730aae2ea58bd2470
  // Client Cookie: 2d84e010d683af48
  // Server Cookie: 010000005f87d5e730aae2ea58bd2470
  constexpr uint8_t kRespFrame2[] = {
      0xc1, 0xbf, 0x85, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06,
      0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x07, 0x64, 0x6e, 0x73, 0x74, 0x65, 0x73,
      0x74, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x51, 0x80, 0x00, 0x04, 0xc0, 0xa8, 0x20, 0xc8,
      0x00, 0x00, 0x29, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x0a,
      0x00, 0x18, 0x2d, 0x84, 0xe0, 0x10, 0xd6, 0x83, 0xaf, 0x48, 0x01, 0x00, 0x00,
      0x00, 0x5f, 0x87, 0xd5, 0xe7, 0x30, 0xaa, 0xe2, 0xea, 0x58, 0xbd, 0x24, 0x70};

  EXPECT_EQ(infer_dns_message(reinterpret_cast<const char*>(kQueryFrame), sizeof(kQueryFrame)),
            kRequest);
  EXPECT_EQ(infer_dns_message(reinterpret_cast<const char*>(kRespFrame), sizeof(kRespFrame)),
            kResponse);

  EXPECT_EQ(infer_dns_message(reinterpret_cast<const char*>(kQueryFrame2), sizeof(kQueryFrame2)),
            kRequest);
  EXPECT_EQ(infer_dns_message(reinterpret_cast<const char*>(kRespFrame2), sizeof(kRespFrame2)),
            kResponse);

  constexpr uint8_t kQueryFrame3[] = "\7\300\1\0\0\1\0\0\0\0\0\0\3www\3cbc\2ca\0\0\1\0\1";

  EXPECT_EQ(infer_dns_message(reinterpret_cast<const char*>(kQueryFrame3), sizeof(kQueryFrame3)),
            kRequest);
}

TEST(ProtocolInferenceTest, Redis) {
  // Captured via ngrep:
  // sudo ngrep -d any port 6379 -x

  constexpr char kReqFrame[] = {0x2a, 0x31, 0x0d, 0x0a, 0x24, 0x38, 0x0d, 0x0a, 0x66,
                                0x6c, 0x75, 0x73, 0x68, 0x61, 0x6c, 0x6c, 0x0d, 0x0a};

  constexpr char kRespFrame[] = {0x2b, 0x4f, 0x4b, 0x0d, 0x0a};

  EXPECT_TRUE(is_redis_message(kReqFrame, sizeof(kReqFrame)));
  EXPECT_TRUE(is_redis_message(kRespFrame, sizeof(kRespFrame)));
}

TEST(ProtocolInferenceTest, Mongo) {
  constexpr uint8_t kReqHeaderFrame[] = {0x4d, 0x01, 0x00, 0x00, 0xd8, 0xe8, 0x91, 0x29,
                                         0x00, 0x00, 0x00, 0x00, 0xd4, 0x07, 0x00, 0x00};
  constexpr uint8_t kRespHeaderFrame[] = {0x4b, 0x00, 0x00, 0x00, 0xf7, 0x4b, 0x9f, 0x29,
                                          0xdb, 0xe8, 0x91, 0x29, 0x01, 0x00, 0x00, 0x00};
  EXPECT_EQ(
      infer_mongo_message(reinterpret_cast<const char*>(kReqHeaderFrame), sizeof(kReqHeaderFrame)),
      kRequest);
  EXPECT_EQ(infer_mongo_message(reinterpret_cast<const char*>(kRespHeaderFrame),
                                sizeof(kRespHeaderFrame)),
            kResponse);
}

TEST(ProtocolInferenceTest, Kafka) {
  struct conn_info_t conn_info = {};

  // Produce API Request Message in a single read.
  constexpr uint8_t kReqFrame[] = {
      0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x63,
      0x6f, 0x6e, 0x73, 0x6f, 0x6c, 0x65, 0x2d, 0x70, 0x72, 0x6f, 0x64, 0x75, 0x63, 0x65, 0x72,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x05, 0xdc, 0x02, 0x12, 0x71, 0x75, 0x69, 0x63, 0x6b,
      0x73, 0x74, 0x61, 0x72, 0x74, 0x2d, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x02, 0x00, 0x00,
      0x00, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e,
      0xff, 0xff, 0xff, 0xff, 0x02, 0xc0, 0xde, 0x91, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8, 0x2d, 0xaa, 0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8, 0x2d,
      0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0x00, 0x00, 0x00, 0x01, 0x38, 0x00, 0x00, 0x00, 0x01, 0x2c, 0x54, 0x68, 0x69, 0x73, 0x20,
      0x69, 0x73, 0x20, 0x6d, 0x79, 0x20, 0x66, 0x69, 0x72, 0x73, 0x74, 0x20, 0x65, 0x76, 0x65,
      0x6e, 0x74, 0x00, 0x00, 0x00, 0x00};

  auto protocol_message =
      infer_protocol(reinterpret_cast<const char*>(kReqFrame), sizeof(kReqFrame), &conn_info);
  EXPECT_EQ(protocol_message.protocol, kProtocolKafka);

  // The Length Header is read first. And then the request body is read.
  constexpr uint8_t kReqHeaderFrame[] = {0x00, 0x00, 0x00, 0x98};
  constexpr uint8_t kReqBodyFrame[] = {
      0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x04, 0x00, 0x10, 0x63, 0x6f, 0x6e, 0x73,
      0x6f, 0x6c, 0x65, 0x2d, 0x70, 0x72, 0x6f, 0x64, 0x75, 0x63, 0x65, 0x72, 0x00, 0x00,
      0x00, 0x01, 0x00, 0x00, 0x05, 0xdc, 0x02, 0x12, 0x71, 0x75, 0x69, 0x63, 0x6b, 0x73,
      0x74, 0x61, 0x72, 0x74, 0x2d, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x73, 0x02, 0x00, 0x00,
      0x00, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x4e, 0xff, 0xff, 0xff, 0xff, 0x02, 0xc0, 0xde, 0x91, 0x11, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x01, 0x7a, 0x1b, 0xc8, 0x2d, 0xaa, 0x00, 0x00, 0x01, 0x7a,
      0x1b, 0xc8, 0x2d, 0xaa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01, 0x38, 0x00, 0x00, 0x00, 0x01, 0x2c,
      0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6d, 0x79, 0x20, 0x66, 0x69, 0x72,
      0x73, 0x74, 0x20, 0x65, 0x76, 0x65, 0x6e, 0x74, 0x00, 0x00, 0x00, 0x00};

  protocol_message = infer_protocol(reinterpret_cast<const char*>(kReqHeaderFrame),
                                    sizeof(kReqHeaderFrame), &conn_info);
  EXPECT_EQ(protocol_message.protocol, kProtocolUnknown);

  protocol_message = infer_protocol(reinterpret_cast<const char*>(kReqBodyFrame),
                                    sizeof(kReqBodyFrame), &conn_info);
  EXPECT_EQ(protocol_message.protocol, kProtocolKafka);
}

TEST(ProtocolInferenceTest, NATS) {
  auto call = [](std::string_view msg) { return infer_nats_message(msg.data(), msg.size()); };

  constexpr std::string_view kTestMessage = "test\r\n";
  EXPECT_EQ(call(kTestMessage), kUnknown);

  constexpr std::string_view kPingMessage = "PING\r\n";
  EXPECT_EQ(call(kPingMessage), kUnknown);

  constexpr std::string_view kPongMessage = "PONG\r\n";
  EXPECT_EQ(call(kPongMessage), kUnknown);

  constexpr std::string_view kConnectMessage = "CONNECT {} \r\n";
  EXPECT_EQ(call(kConnectMessage), kRequest);

  constexpr std::string_view kPubMessage = "PUB {} \r\n";
  EXPECT_EQ(call(kPubMessage), kRequest);

  constexpr std::string_view kSubMessage = "SUB {} \r\n";
  EXPECT_EQ(call(kSubMessage), kRequest);

  constexpr std::string_view kUnsubMessage = "UNSUB {} \r\n";
  EXPECT_EQ(call(kUnsubMessage), kRequest);

  constexpr std::string_view kInfoMessage = "INFO {} \r\n";
  EXPECT_EQ(call(kInfoMessage), kResponse);

  constexpr std::string_view kMsgMessage = "MSG {} \r\n";
  EXPECT_EQ(call(kMsgMessage), kResponse);

  constexpr std::string_view kOKMessage = "+OK {} \r\n";
  EXPECT_EQ(call(kOKMessage), kResponse);

  constexpr std::string_view kERRMessage = "-ERR {} \r\n";
  EXPECT_EQ(call(kERRMessage), kResponse);
}
