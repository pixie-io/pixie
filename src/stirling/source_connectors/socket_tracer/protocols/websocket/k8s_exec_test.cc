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

#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>

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

constexpr std::string_view kKubeletTTYPath =
    "/exec/ws-demo/ws-demo/ws-demo?command=sh&input=1&output=1&tty=1";
constexpr std::string_view kStatusSuccess = R"({"metadata":{},"status":"Success"})";

Message Msg(uint64_t timestamp_ns, message_type_t type, std::string_view payload,
            Opcode opcode = Opcode::kBinary) {
  Message msg;
  msg.timestamp_ns = timestamp_ns;
  msg.type = type;
  msg.opcode = opcode;
  msg.fragments = 1;
  msg.payload = std::string(payload);
  msg.payload_size = msg.payload.size();
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
      In(12,
         "\x00"
         "cat"sv),
      Out(13,
          "\x01"
          "cat"),
      In(14, "\x00 /etc/hostname"sv),
      Out(15, "\x01 /etc/hostname"),
      In(16, "\x00\r"sv),
      Out(17, "\x01\r\n"),
      Out(18, "\x01ws-demo\r\n/ # "),
      In(19,
         "\x00"
         "exi"sv),
      Out(20,
          "\x01"
          "exi"),
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
      In(1, "\x00lx"sv),
      Out(2, "\x01lx"),
      In(3, "\x00\x7f"sv),
      Out(4, "\x01\b \b"),
      In(5, "\x00s"sv),
      Out(6, "\x01s"),
      In(7, "\x00\r"sv),
      Out(8, "\x01\r\nbin\r\n/ # "),
      In(9, "\x00junk"sv),
      Out(10, "\x01junk"),
      In(11, "\x00\x15"sv),
      In(12, "\x00pwd\r"sv),
      Out(13, "\x01\r\n/\r\n/ # "),
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
      In(1,
         "\x00"
         "cat /etc/hosts"sv),
      Out(2,
          "\x01"
          "cat /etc/hosts"),
      In(3, "\x00\x03"sv),
      Out(4, "\x01^C\r\n/ # "),
      In(5, "\x00\x04"sv),
      Out(6, absl::StrCat("\x03", kStatusSuccess)),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, std::move(messages), &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "cat /etc/hosts^C", "^C\n"),
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

  DecodeK8sExec(kKubeletTTYPath, {Out(1, "\x01/ # "), In(2, "\x00i"sv), Out(3, "\x01i")}, &state,
                &records);
  DecodeK8sExec(kKubeletTTYPath,
                {In(4,
                    "\x00"
                    "d\r"sv),
                 Out(5, "\x01\r\nuid=0\r\n/ # ")},
                &state, &records);
  DecodeK8sExec(kKubeletTTYPath, {Out(6, absl::StrCat("\x03", kStatusSuccess))}, &state, &records);

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

std::string TarHeader(std::string_view name, uint64_t mode, uint64_t size, char typeflag = '0') {
  std::string header(512, '\0');
  header.replace(0, name.size(), name);
  const std::string mode_field = absl::StrFormat("%07o", mode);
  header.replace(100, mode_field.size(), mode_field);
  const std::string size_field = absl::StrFormat("%011o", size);
  header.replace(124, size_field.size(), size_field);
  header[156] = typeflag;
  header.replace(257, 6, std::string("ustar\0", 6));
  header.replace(263, 2, "00");
  return header;
}

std::string TarEntry(std::string_view name, uint64_t mode, std::string_view content) {
  std::string entry = TarHeader(name, mode, content.size());
  entry.append(content);
  entry.append((512 - content.size() % 512) % 512, '\0');
  return entry;
}

std::string TarArchive(const std::vector<std::string>& entries) {
  std::string archive = absl::StrJoin(entries, "");
  archive.append(1024, '\0');
  return archive;
}

TEST(K8sExecTest, TarFromStdoutBecomesManifest) {
  constexpr std::string_view kPath =
      "/api/v1/namespaces/ns/pods/pod/exec?command=tar&command=cf&command=-&command=%2Fetc&"
      "stdout=true";
  const std::string archive =
      TarArchive({TarEntry("etc/passwd", 0644, "root:x:0:0:root:/root:/bin/sh\n"),
                  TarEntry("etc/group", 0640, std::string(700, 'x'))});
  std::vector<Message> messages = {Out(1, "\x01")};
  for (size_t pos = 0; pos < archive.size(); pos += 300) {
    messages.push_back(Out(2 + pos, absl::StrCat("\x01", archive.substr(pos, 300))));
  }
  messages.push_back(Out(10000, absl::StrCat("\x03", kStatusSuccess)));
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kPath, std::move(messages), &state, &records);

  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[1].kind, RecordKind::kExit);
  EXPECT_EQ(records[1].output,
            absl::StrCat("etc/passwd mode=0644 size=30 head=\"root:x:0:0:root:/root:/bin/sh\\n\"\n",
                         "etc/group mode=0640 size=700 head=\"", std::string(64, 'x'), "\"\n"));
  EXPECT_EQ(records[1].output_size, archive.size());
}

TEST(K8sExecTest, TarLongNameIsResolved) {
  constexpr std::string_view kPath = "/exec/ns/pod/ctr?command=tar&command=cf&command=-&output=1";
  const std::string long_name = absl::StrCat("data/", std::string(120, 'n'));
  std::string long_name_entry = TarHeader("././@LongLink", 0, long_name.size() + 1, 'L');
  long_name_entry.append(long_name);
  long_name_entry.push_back('\0');
  long_name_entry.append((512 - (long_name.size() + 1) % 512) % 512, '\0');
  const std::string archive = TarArchive({long_name_entry, TarEntry("data/nnn", 0600, "v")});
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(
      kPath, {Out(1, absl::StrCat("\x01", archive)), Out(2, absl::StrCat("\x03", kStatusSuccess))},
      &state, &records);

  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[1].output, absl::StrCat(long_name, " mode=0600 size=1 head=\"v\"\n"));
}

TEST(K8sExecTest, NonTTYStdinIsOneInputRow) {
  constexpr std::string_view kPath = "/exec/ns/pod/ctr?command=sh&input=1&output=1";
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kPath,
                {In(1, "\x00id\n"sv), In(2, "\x00hostname\n"sv), Out(3, "\x01uid=0\npod\n"),
                 Out(4, absl::StrCat("\x03", kStatusSuccess))},
                &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "id\nhostname\n", ""),
                                   IsRecord(RecordKind::kExit, kStatusSuccess, "uid=0\npod\n")));
  EXPECT_EQ(records[1].msg.timestamp_ns, 1U);
  EXPECT_EQ(records[1].msg.payload_size, 12U);
}

TEST(K8sExecTest, TarOnStdinBecomesManifest) {
  constexpr std::string_view kPath =
      "/api/v1/namespaces/ns/pods/pod/exec?command=tar&command=xmf&command=-&command=-C&"
      "command=%2Ftmp&stdin=true";
  const std::string archive = TarArchive({TarEntry("payload.sh", 0755, "#!/bin/sh\necho hi\n")});
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kPath,
                {In(1, absl::StrCat(std::string(1, '\0'), archive)), In(2, "\xff\x00"sv),
                 Out(3, absl::StrCat("\x03", kStatusSuccess))},
                &state, &records);

  EXPECT_THAT(
      records,
      ElementsAre(IsRecord(RecordKind::kExec, "tar xmf - -C /tmp", ""),
                  IsRecord(RecordKind::kInput,
                           "payload.sh mode=0755 size=18 head=\"#!/bin/sh\\necho hi\\n\"\n", ""),
                  IsRecord(RecordKind::kExit, kStatusSuccess, "")));
  EXPECT_EQ(records[1].msg.payload_size, archive.size());
}

TEST(K8sExecTest, EchoOffInputIsRedacted) {
  std::vector<Message> messages = {
      Out(1, "\x01/ # "),
      In(2, "\x00su"sv),
      Out(3, "\x01su"),
      In(4, "\x00\r"sv),
      Out(5, "\x01\r\nPassword: "),
      In(6, "\x00hunter2"sv),
      In(7, "\x00\r"sv),
      Out(8, "\x01\r\n/ # "),
      Out(9, absl::StrCat("\x03", kStatusSuccess)),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, std::move(messages), &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "su", ""),
                                   IsRecord(RecordKind::kInput, "<redacted: no echo>", ""),
                                   IsRecord(RecordKind::kExit, kStatusSuccess, "")));
}

TEST(K8sExecTest, PastedLineKeepsCommandAndDropsEcho) {
  std::vector<Message> messages = {
      Out(1, "\x01/ # "),
      In(2, "\x00ls\r"sv),
      Out(3, "\x01ls\r\nbin\r\n/ # "),
      Out(4, absl::StrCat("\x03", kStatusSuccess)),
  };
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath, std::move(messages), &state, &records);

  EXPECT_THAT(records, ElementsAre(IsRecord(RecordKind::kExec, "sh", ""),
                                   IsRecord(RecordKind::kInput, "ls", "bin\n"),
                                   IsRecord(RecordKind::kExit, kStatusSuccess, "")));
}

TEST(K8sExecTest, InputLineIsBounded) {
  const std::string typed(10000, 'a');
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kKubeletTTYPath,
                {In(1, absl::StrCat(std::string(1, '\0'), typed)),
                 Out(2,
                     "\x01"
                     "aaaa"),
                 In(3, "\x00\r"sv), Out(4, absl::StrCat("\x03", kStatusSuccess))},
                &state, &records);

  ASSERT_EQ(records.size(), 3U);
  EXPECT_EQ(records[1].kind, RecordKind::kInput);
  EXPECT_EQ(records[1].msg.payload, std::string(4096, 'a'));
}

std::string TarTwoFiles() {
  return TarArchive(
      {TarEntry("big.bin", 0644, std::string(5000, 'b')), TarEntry("small.txt", 0600, "hi")});
}

constexpr std::string_view kTarStdoutPath =
    "/api/v1/namespaces/ns/pods/pod/exec?command=tar&command=cf&command=-&command=%2Fdata&"
    "stdout=true";

Message TruncatedOut(uint64_t timestamp_ns, std::string_view data, size_t kept) {
  Message msg = Out(timestamp_ns, absl::StrCat("\x01", data.substr(0, kept)));
  msg.payload_size = 1 + data.size();
  return msg;
}

TEST(K8sExecTest, TruncatedTarContentKeepsManifest) {
  const std::string archive = TarTwoFiles();
  constexpr size_t kFirstMessage = 512 + 5120;
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kTarStdoutPath,
                {TruncatedOut(1, archive.substr(0, kFirstMessage), 612),
                 Out(2, absl::StrCat("\x01", archive.substr(kFirstMessage))),
                 Out(3, absl::StrCat("\x03", kStatusSuccess))},
                &state, &records);

  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[1].output,
            absl::StrCat("big.bin mode=0644 size=5000 head=\"", std::string(64, 'b'), "\"\n",
                         "small.txt mode=0600 size=2 head=\"hi\"\n"));
  EXPECT_EQ(records[1].output_size, archive.size());
}

TEST(K8sExecTest, TarLossAcrossHeaderIsMarked) {
  const std::string archive = TarTwoFiles();
  constexpr size_t kFirstMessage = 512 + 5120 + 100;
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(kTarStdoutPath,
                {TruncatedOut(1, archive.substr(0, kFirstMessage), 612),
                 Out(2, absl::StrCat("\x01", archive.substr(kFirstMessage))),
                 Out(3, absl::StrCat("\x03", kStatusSuccess))},
                &state, &records);

  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[1].output, absl::StrCat("big.bin mode=0644 size=5000 head=\"",
                                            std::string(64, 'b'), "\"\n", "[TRUNCATED]\n"));
}

TEST(K8sExecTest, TarManifestIsBounded) {
  std::vector<std::string> entries;
  for (int i = 0; i < 200; ++i) {
    entries.push_back(TarEntry(absl::StrCat("dir/", std::string(90, 'f'), i), 0644, "x"));
  }
  const std::string archive = TarArchive(entries);
  ExecState state;
  std::vector<Record> records;

  DecodeK8sExec(
      kTarStdoutPath,
      {Out(1, absl::StrCat("\x01", archive)), Out(2, absl::StrCat("\x03", kStatusSuccess))}, &state,
      &records);

  ASSERT_EQ(records.size(), 2U);
  EXPECT_LE(records[1].output.size(), 4096U);
  EXPECT_TRUE(absl::StartsWith(records[1].output, absl::StrCat("dir/", std::string(90, 'f'), "0")));
  EXPECT_EQ(records[1].output_size, archive.size());
}

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
