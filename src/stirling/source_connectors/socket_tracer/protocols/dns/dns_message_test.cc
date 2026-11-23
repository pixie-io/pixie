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

#include "src/stirling/source_connectors/socket_tracer/protocols/dns/dns_message.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "src/stirling/source_connectors/socket_tracer/protocols/dns/dns_message_testdata.h"

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

using testdata::Bytes;

TEST(WalkDNSMessageTest, PTRQuery) {
  const std::string buf = Bytes(testdata::kPTRQueryHex);
  DNSMessageInfo info;
  ASSERT_TRUE(WalkDNSMessage(buf, &info));
  EXPECT_EQ(info.length, buf.size());
  ASSERT_EQ(info.questions.size(), 1);
  EXPECT_EQ(info.questions[0].name, "12.0.42.10.in-addr.arpa");
  EXPECT_EQ(info.questions[0].qtype, 12);
  EXPECT_TRUE(info.answers.empty());
}

TEST(WalkDNSMessageTest, PTRResponse) {
  const std::string buf = Bytes(testdata::kPTRRespHex);
  DNSMessageInfo info;
  ASSERT_TRUE(WalkDNSMessage(buf, &info));
  EXPECT_EQ(info.length, buf.size());
  EXPECT_EQ(info.txid, 0x1234);
  ASSERT_EQ(info.answers.size(), 1);
  EXPECT_EQ(info.answers[0].type, kDNSTypePTR);
  EXPECT_EQ(info.answers[0].name, "12.0.42.10.in-addr.arpa");
  EXPECT_EQ(info.answers[0].target, "10-42-0-12.kube-dns.kube-system.svc.cluster.local");
}

TEST(WalkDNSMessageTest, CompressedNamesAndAdditionalRecord) {
  const std::string buf = Bytes(testdata::kCompressedRespHex);
  DNSMessageInfo info;
  ASSERT_TRUE(WalkDNSMessage(buf, &info));
  EXPECT_EQ(info.length, buf.size());
  ASSERT_EQ(info.questions.size(), 1);
  EXPECT_EQ(info.questions[0].name, "intellij-experiments.appspot.com");
  EXPECT_EQ(info.questions[0].qtype, 1);
  ASSERT_EQ(info.answers.size(), 1);
  EXPECT_EQ(info.answers[0].name, "intellij-experiments.appspot.com");
  EXPECT_EQ(info.answers[0].type, kDNSTypeA);
  EXPECT_EQ(info.answers[0].addr, std::string("\xd8\x3a\xc2\xb4", 4));
}

TEST(WalkDNSMessageTest, FindsBoundaryOfBackToBackMessages) {
  const std::string first = Bytes(testdata::kPTRQueryHex);
  const std::string second = Bytes(testdata::kAQueryHex);
  const std::string buf = first + second + first;
  DNSMessageInfo info;
  ASSERT_TRUE(WalkDNSMessage(buf, &info));
  EXPECT_EQ(info.length, first.size());

  std::string_view rest(buf);
  rest.remove_prefix(info.length);
  ASSERT_TRUE(WalkDNSMessage(rest, &info));
  EXPECT_EQ(info.length, second.size());
  EXPECT_EQ(info.questions[0].name, "kube-dns.kube-system.svc.cluster.local");
}

TEST(WalkDNSMessageTest, RejectsTruncatedMessages) {
  const std::string buf = Bytes(testdata::kPTRRespHex);
  DNSMessageInfo info;
  for (size_t cut = 1; cut < buf.size(); ++cut) {
    EXPECT_FALSE(WalkDNSMessage(std::string_view(buf).substr(0, buf.size() - cut), &info))
        << "cut=" << cut;
  }
}

TEST(WalkDNSMessageTest, RejectsPointerLoop) {
  const std::string buf("\x00\x01\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\xc0\x0c\x00\x01\x00\x01",
                        18);
  DNSMessageInfo info;
  EXPECT_FALSE(WalkDNSMessage(buf, &info));
}

TEST(WalkDNSMessageTest, TypeNames) {
  EXPECT_EQ(DNSTypeName(12), "PTR");
  EXPECT_EQ(DNSTypeName(16), "TXT");
  EXPECT_EQ(DNSTypeName(4242), "TYPE4242");
}

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
