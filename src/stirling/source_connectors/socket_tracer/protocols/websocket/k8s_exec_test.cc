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

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/k8s_exec.h"

#include <string>
#include <utility>
#include <vector>

#include <absl/strings/str_cat.h>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace websocket {

using std::string_view_literals::operator""sv;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;

constexpr std::string_view kKubeletTTYPath = "/exec/ws-demo/ws-demo/ws-demo?command=sh&input=1&output=1&tty=1";
constexpr std::string_view kStatusSuccess = R"({"metadata":{},"status":"Success"})";

Message Msg(uint64_t timestamp_ns, message_type_t type, std::string_view payload,
            Opcode opcode = Opcode::kBinary) {
  Message msg;
  msg.timestamp_ns = timestamp_ns;
  msg.type = type;
  msg.opcode = opcode;
  msg.fragments = 1;
  msg.payload = std::string(payload);
  return msg;
}

Message In(uint64_t timestamp_ns, std::string_view payload) {
  return Msg(timestamp_ns, message_type_t::kRequest, payload);
}

Message Out(uint64_t timestamp_ns, std::string_view payload) {
  return Msg(timestamp_ns, message_type_t::kResponse, payload);
}

auto IsRecord(RecordKind kind, std::string_view payload, std::string_view output) {
  return AllOf(Field(&Record::kind, kind),
               Field(&Record::msg, Field(&Message::payload, std::string(payload))),
               Field(&Record::output, std::string(output)));
}

TEST(K8sExecTest, ChannelSubprotocols) {
  EXPECT_TRUE(IsK8sChannelProtocol("v5.channel.k8s.io"));
  EXPECT_TRUE(IsK8sChannelProtocol("v4.channel.k8s.io"));
  EXPECT_TRUE(IsK8sChannelProtocol("channel.k8s.io"));
  EXPECT_FALSE(IsK8sChannelProtocol("base64.channel.k8s.io"));
  EXPECT_FALSE(IsK8sChannelProtocol("v4.base64.channel.k8s.io"));
  EXPECT_FALSE(IsK8sChannelProtocol("SPDY/3.1+portforward.k8s.io"));
  EXPECT_FALSE(IsK8sChannelProtocol("graphql-transport-ws"));
  EXPECT_FALSE(IsK8sChannelProtocol(""));
}

TEST(K8sExecTest, CommandFromURL) {
  EXPECT_EQ(K8sExecCommand("/api/v1/namespaces/ws-demo/pods/ws-demo/exec?command=tar&command=cf&"
                           "command=-&command=%2Fetc%2Fpasswd&container=ws-demo&stderr=true&"
                           "stdout=true"),
            "tar cf - /etc/passwd");
  EXPECT_EQ(K8sExecCommand(kKubeletTTYPath), "sh");
  EXPECT_EQ(K8sExecCommand("/exec/ns/pod/ctr?command=echo&command=a+b"), "echo a b");
  EXPECT_EQ(K8sExecCommand("/api/v1/namespaces/ns/pods/pod/attach?container=c"), "");
}

TEST(K8sExecTest, TTYFromURL) {
  EXPECT_TRUE(K8sExecTTY(kKubeletTTYPath));
  EXPECT_TRUE(K8sExecTTY("/api/v1/namespaces/ns/pods/pod/exec?command=sh&tty=true&stdin=true"));
  EXPECT_FALSE(K8sExecTTY("/api/v1/namespaces/ns/pods/pod/exec?command=ls&stdout=true"));
  EXPECT_FALSE(K8sExecTTY("/exec/ns/pod/ctr?command=sh&tty=0"));
}

TEST(K8sExecTest, CleanTerminalOutput) {
  EXPECT_EQ(CleanTerminalOutput("\r\nuid=0(root) gid=0(root)\r\n/ # ", true),
            "uid=0(root) gid=0(root)\n");
  EXPECT_EQ(CleanTerminalOutput("\r\n\x1b[1;34mbin\x1b[m    \x1b[1;34metc\x1b[m\r\n/ # ", true),
            "bin    etc\n");
  EXPECT_EQ(CleanTerminalOutput("\r\n/ # ", true), "");
  EXPECT_EQ(CleanTerminalOutput("\x1b]0;title\x07ok\r\n$ ", true), "ok\n");
  EXPECT_EQ(CleanTerminalOutput("a\nb", false), "a\nb");
}

TEST(K8sExecTest, PrintableEscapesOnlyNonText) {
  EXPECT_EQ(Printable("ls -la\n\tx"), "ls -la\n\tx");
  EXPECT_EQ(Printable("\x01hi"), "\\x01hi");
}

TEST(K8sExecTest, TTYSessionBecomesOneRowPerCommand) {
  std::vector<Message> messages = {
      Out(1, "\x01"),
      In(2, "\x04{\"Width\":0,\"Height\":0}\n"),
      Out(3, "\x01/ # "),
      In(4, "\x00id"sv),
      Out(5, "\x01id"),
      In(6, "\x00\r"sv),
      Out(7, "\x01\r\n"),
      Out(8, "\x01uid=0(root) gid=0(root) groups=0(root),10(wheel)\r\n"),
      Out(9, "\x01/ # "),
      Msg(10, message_type_t::kRequest, "", Opcode::kPing),
      Msg(11, message_type_t::kResponse, "", Opcode::kPong),
      In(12, "\x00""cat"sv),
      Out(13, "\x01""cat"),
      In(14, "\x00 /etc/hostname"sv),
      Out(15, "\x01 /etc/hostname"),
      In(16, "\x00\r"sv),
      Out(17, "\x01\r\n"),
      Out(18, "\x01ws-demo\r\n/ # "),
      In(19, "\x00""exi"sv),
      Out(20, "\x01""exi"),
      In(21, "\x00t"sv),
      Out(22, "\x01t"),
      In(23, "\x00\r"sv),
      Out(24, "\x01\r\n"),
      Out(25, absl::StrCat("\x03", kStatusSuccess)),
      Msg(26, message_type_t::kResponse, "\x03\xe8", Opcode::kClose),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, std::move(messages), &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "id",
                                            "uid=0(root) gid=0(root) groups=0(root),10(wheel)\n"),
                                   IsRecord(RecordKind::kInput, "cat /etc/hostname", "ws-demo\n"),
                                   IsRecord(RecordKind::kInput, "exit", ""),
                                   IsRecord(RecordKind::kExit, kStatusSuccess, "")));
  EXPECT_EQ(records[0].msg.timestamp_ns, 1U);
  EXPECT_EQ(records[1].msg.timestamp_ns, 4U);
  EXPECT_EQ(records[2].msg.timestamp_ns, 12U);
  EXPECT_EQ(records[4].msg.timestamp_ns, 25U);
}

TEST(K8sExecTest, LineEditingIsApplied) {
  std::vector<Message> messages = {
      In(1, "\x00lx"sv),     In(2, "\x00\x7f"sv), In(3, "\x00s\r"sv), Out(4, "\x01\r\nbin\r\n/ # "),
      In(5, "\x00junk"sv),   In(6, "\x00\x15"sv), In(7, "\x00pwd\r"sv),
      Out(8, "\x01\r\n/\r\n/ # "),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, std::move(messages), &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "ls", "bin\n")));
  EXPECT_EQ(state.pending->line, "pwd");
}

TEST(K8sExecTest, AbandonedLineIsKept) {
  std::vector<Message> messages = {
      In(1, "\x00""cat /etc/shadow"sv),
      In(2, "\x00\x03"sv),
      Out(3, "\x01^C\r\n/ # "),
      In(4, "\x00\x04"sv),
      Out(5, absl::StrCat("\x03", kStatusSuccess)),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, std::move(messages), &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "cat /etc/shadow^C", "^C\n"),
                                   IsRecord(RecordKind::kInput, "^D", ""),
                                   IsRecord(RecordKind::kExit, kStatusSuccess, "")));
}

TEST(K8sExecTest, NonTTYOutputGoesToExitRow) {
  constexpr std::string_view kPath =
      "/api/v1/namespaces/ws-demo/pods/ws-demo/exec?command=tar&command=cf&command=-&"
      "command=%2Fetc%2Fpasswd&container=ws-demo&stderr=true&stdout=true";
  const std::string tar = absl::StrCat("etc/passwd", std::string(4, '\0'));
  std::vector<Message> messages = {
      Out(1, "\x01"),
      Out(2, "\x02tar: removing leading '/' from member names\n"),
      Out(3, absl::StrCat("\x01", tar)),
      Out(4, absl::StrCat("\x03", kStatusSuccess)),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kPath, std::move(messages), &state, &records);

  ASSERT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "tar cf - /etc/passwd", ""),
                                   IsRecord(RecordKind::kExit, kStatusSuccess,
                                            absl::StrCat("tar: removing leading '/' from member "
                                                         "names\n",
                                                         tar))));
  EXPECT_EQ(records[1].output_size, 44U + tar.size());
}

TEST(K8sExecTest, SessionContinuesAcrossCalls) {
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, {Out(1, "\x01/ # "), In(2, "\x00i"sv)}, &state, &records);
  DecodeK8sExec(kKubeletTTYPath, {In(3, "\x00""d\r"sv), Out(4, "\x01\r\nuid=0\r\n/ # ")}, &state,
                &records);
  DecodeK8sExec(kKubeletTTYPath, {Out(5, absl::StrCat("\x03", kStatusSuccess))}, &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "id", "uid=0\n"),
                                   IsRecord(RecordKind::kExit, kStatusSuccess, "")));
  EXPECT_EQ(records[1].msg.timestamp_ns, 2U);
}

TEST(K8sExecTest, BufferedOutputIsBounded) {
  constexpr std::string_view kPath = "/exec/ns/pod/ctr?command=cat&command=big&output=1";
  const std::string chunk = absl::StrCat("\x01", std::string(100000, 'a'));
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kPath, {Out(1, chunk), Out(2, absl::StrCat("\x03", kStatusSuccess))}, &state,
                &records);

  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[1].output_size, 100000U);
  EXPECT_LT(records[1].output.size(), 100000U);
  EXPECT_THAT(state.output, IsEmpty());
}

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
