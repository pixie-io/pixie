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

#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/redis_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/redis_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"

namespace px {
namespace stirling {

using ::px::SubProcess;

using testing::ContainsWithRelativeOrder;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::StrEq;

using ::px::stirling::testing::FindRecordIdxMatchesPID;

using ::px::stirling::testing::RedisClientContainer;
using ::px::stirling::testing::RedisContainer;

struct RedisTraceTestCase {
  std::string cmd;
  std::string exp_cmd;
  std::string exp_req;
  std::string exp_resp;
};

class RedisTraceBPFTest : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false>,
                          public ::testing::WithParamInterface<RedisTraceTestCase> {
 protected:
  RedisTraceBPFTest() { PX_CHECK_OK(container_.Run(std::chrono::seconds{150})); }

  RedisContainer container_;
};

struct RedisTraceRecord {
  std::string cmd;
  std::string req;
  std::string resp;
};

std::ostream& operator<<(std::ostream& os, const RedisTraceRecord& record) {
  os << "cmd: " << record.cmd << " req: " << record.req << " resp: " << record.resp;
  return os;
}

bool operator==(const RedisTraceRecord& lhs, const RedisTraceRecord& rhs) {
  return lhs.cmd == rhs.cmd && lhs.req == rhs.req && lhs.resp == rhs.resp;
}

std::vector<RedisTraceRecord> GetRedisTraceRecords(
    const types::ColumnWrapperRecordBatch& record_batch, int pid) {
  std::vector<RedisTraceRecord> res;
  for (const auto& idx : FindRecordIdxMatchesPID(record_batch, kRedisUPIDIdx, pid)) {
    res.push_back(
        RedisTraceRecord{std::string(record_batch[kRedisCmdIdx]->Get<types::StringValue>(idx)),
                         std::string(record_batch[kRedisReqIdx]->Get<types::StringValue>(idx)),
                         std::string(record_batch[kRedisRespIdx]->Get<types::StringValue>(idx))});
  }
  return res;
}

// Verifies that batched commands can be traced correctly.
TEST_F(RedisTraceBPFTest, VerifyBatchedCommands) {
  StartTransferDataThread();

  // NOTE: select 0 must be the last one in order to avoid mess up with the key lookup in the
  // storage index.
  constexpr std::string_view kRedisCmds = R"(
    ping test
    set foo 100 EX 10 NX
    expire foo 10000
    bitcount foo 0 0
    incr foo
    append foo xxx
    get foo
    mget foo bar
    sadd myset 1 2 3
    sscan myset 0 MATCH [a-z]+ COUNT 10
    scard myset
    smembers myset
    hmset fooset f1 100 f2 200
    hmget fooset f1 f2
    hget fooset f1
    hgetall fooset
    watch foo bar
    unwatch
    select 0
  )";

  RedisClientContainer redis_cli_client;
  ASSERT_OK_AND_ASSIGN(
      std::string output,
      redis_cli_client.Run(
          std::chrono::seconds{60},
          {absl::Substitute("--network=container:$0", container_.container_name())},
          {"bash", "-c", absl::Substitute("echo '$0' | redis-cli", kRedisCmds)}));
  redis_cli_client.Wait();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kRedisTableNum);

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  std::vector<RedisTraceRecord> redis_trace_records =
      GetRedisTraceRecords(record_batch, container_.process_pid());

  ContainsWithRelativeOrder(
      redis_trace_records, RedisTraceRecord{"PING", R"({"message":"test"})", "test"},
      RedisTraceRecord{"SET", R"({"key":"foo","value":"100","options":["EX 10","NX"]})", "OK"},
      RedisTraceRecord{"EXPIRE", R"({"key":"foo","seconds":"10000"})", "1"},
      RedisTraceRecord{"BITCOUNT", R"(["foo","0","0"])", "3"},
      RedisTraceRecord{"INCR", R"({"key":"foo"})", "101"},
      RedisTraceRecord{"APPEND", R"({"key":"foo","value":"xxx"})", "6"},
      RedisTraceRecord{"GET", R"({"key":"foo"})", "101xxx"},
      RedisTraceRecord{"MGET", R"({"key":["foo","bar"]})", R"(["101xxx","<NULL>"])"},
      RedisTraceRecord{"SADD", R"({"key":"myset","member":["1","2","3"]})", "3"},
      RedisTraceRecord{"SSCAN", R"({"key":"myset","cursor":"0","pattern":"[a-z]+","count":"10"})",
                       R"(["0","[]"])"},
      RedisTraceRecord{"SCARD", R"({"key":"myset"})", "3"},
      RedisTraceRecord{"SMEMBERS", R"({"key":"myset"})", R"(["1","2","3"])"},
      RedisTraceRecord{"HMSET",
                       R"({"key":"fooset","field value":[{"field":"f1"},)"
                       R"({"value":"100"},{"field":"f2"},{"value":"200"}]})",
                       "OK"},
      RedisTraceRecord{"HMGET", R"({"key":"fooset","field":["f1","f2"]})", R"(["100","200"])"},
      RedisTraceRecord{"HGET", R"({"key":"fooset","field":"f1"})", "100"},
      RedisTraceRecord{"HGETALL", R"({"key":"fooset"})", R"(["f1","100","f2","200"])"},
      RedisTraceRecord{"WATCH", R"({"key":["foo","bar"]})", "OK"},
      RedisTraceRecord{"UNWATCH", "[]", "OK"},
      RedisTraceRecord{"SELECT", R"({"index":"0"})", "OK"});
}

// Verifies that pub/sub commands can be traced correctly.
TEST_F(RedisTraceBPFTest, VerifyPubSubCommands) {
  StartTransferDataThread();
  std::string output;
  RedisClientContainer redis_sub_client;
  ASSERT_OK(redis_sub_client.Run(
      std::chrono::seconds{60},
      {absl::Substitute("--network=container:$0", container_.container_name())},
      {"redis-cli", "subscribe", "foo"}));

  RedisClientContainer redis_pub_client;
  ASSERT_OK(redis_pub_client.Run(
      std::chrono::seconds{60},
      {absl::Substitute("--network=container:$0", container_.container_name())},
      {"redis-cli", "publish", "foo", "test"}));

  redis_pub_client.Wait();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kRedisTableNum);

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  std::vector<RedisTraceRecord> redis_trace_records =
      GetRedisTraceRecords(record_batch, container_.process_pid());

  ContainsWithRelativeOrder(
      redis_trace_records, RedisTraceRecord{"SUBSCRIBE", R"({"channel":["foo","1"]})", ""},
      RedisTraceRecord{"PUBLISH", R"({"channel":"foo","message":"test"})", "1"},
      RedisTraceRecord{"PUSH PUB", "", R"(["message","foo","test"])"});
}

// Verifies that script load and evalsha works as expected.
//
// We need to test this separately because we need the returned script sha from script load
// to assemble the evalsha command.
TEST_F(RedisTraceBPFTest, ScriptLoadAndEvalSHA) {
  StartTransferDataThread();
  RedisClientContainer script_load_container;
  std::string sha;
  ASSERT_OK_AND_ASSIGN(
      sha, script_load_container.Run(
               std::chrono::seconds{60},
               {absl::Substitute("--network=container:$0", container_.container_name())},
               {"redis-cli", "script", "load", "return 1"}));
  script_load_container.Wait(false);
  ASSERT_OK(script_load_container.Stdout(&sha));
  absl::StripAsciiWhitespace(&sha);
  ASSERT_FALSE(sha.empty());

  RedisClientContainer eval_sha_container;
  ASSERT_OK_AND_ASSIGN(
      std::string output,
      eval_sha_container.Run(
          std::chrono::seconds{60},
          {absl::Substitute("--network=container:$0", container_.container_name())},
          {"redis-cli", "evalsha", sha, "2", "1", "1", "2", "2"}));
  eval_sha_container.Wait(false);
  ASSERT_OK(eval_sha_container.Stdout(&output));
  ASSERT_FALSE(output.empty());

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kRedisTableNum);

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  std::vector<RedisTraceRecord> redis_trace_records =
      GetRedisTraceRecords(record_batch, container_.process_pid());

  ContainsWithRelativeOrder(
      redis_trace_records, RedisTraceRecord{"SCRIPT LOAD", R"({"script":"return 1"})", sha},
      RedisTraceRecord{
          "EVALSHA",
          absl::Substitute(R"({"sha1":"$0","numkeys":"2","key":["1","1"],"value":["2","2"]})", sha),
          "1"});
}

// Verifies individual commands.
TEST_P(RedisTraceBPFTest, VerifyCommand) {
  StartTransferDataThread();

  std::string_view redis_cmd = GetParam().cmd;
  RedisClientContainer redis_cli_client;
  ASSERT_OK_AND_ASSIGN(std::string output,
                       redis_cli_client.Run(std::chrono::seconds{60},
                                            {absl::Substitute("--network=container:$0",
                                                              container_.container_name())},
                                            {"bash", "-c", "redis-cli " + std::string(redis_cmd)}));
  redis_cli_client.Wait();
  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kRedisTableNum);

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  ContainsWithRelativeOrder(
      GetRedisTraceRecords(record_batch, container_.process_pid()),
      RedisTraceRecord{GetParam().exp_cmd, GetParam().exp_req, GetParam().exp_resp});
}

INSTANTIATE_TEST_SUITE_P(
    Commands, RedisTraceBPFTest,
    // Add new commands here.
    ::testing::Values(
        RedisTraceTestCase{"lpush ilist 100", "LPUSH", R"({"key":"ilist","element":["100"]})", "1"},
        RedisTraceTestCase{"rpush ilist 200", "RPUSH", R"({"key":"ilist","element":["200"]})", "1"},
        RedisTraceTestCase{"lrange ilist 0 1", "LRANGE",
                           R"({"key":"ilist","start":"0","stop":"1"})", "[]"},
        RedisTraceTestCase{"flushall", "FLUSHALL", "[]", "OK"}));

}  // namespace stirling
}  // namespace px
