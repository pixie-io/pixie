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

#include <string>

#include <absl/strings/str_replace.h>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace kafka = protocols::kafka;

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::SocketTraceBPFTest;
using ::px::testing::BazelBinTestFilePath;
using ::px::testing::TestFilePath;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::StrEq;
using ::px::operator<<;

class KafkaContainer : public ContainerRunner {
 public:
  KafkaContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/kafka_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "kafka_server";
  static constexpr std::string_view kReadyMessage =
      "Recorded new controller, from now on will use broker";
};

class ZooKeeperContainer : public ContainerRunner {
 public:
  ZooKeeperContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/source_connectors/socket_tracer/testing/containers/zookeeper_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "zookeeper_server";
  static constexpr std::string_view kReadyMessage = "INFO PrepRequestProcessor (sid:0) started";
};

class KafkaTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ true> {
 protected:
  KafkaTraceTest() {
    // Run Zookeeper.
    StatusOr<std::string> zookeeper_run_result = zookeeper_server_.Run(
        std::chrono::seconds{90},
        {"--name=zookeeper", "--env=ZOOKEEPER_CLIENT_PORT=32181", "--env=ZOOKEEPER_TICK_TIME=2000",
         "--env=ZOOKEEPER_SYNC_LIMIT=2", "--env=ZOOKEEPER_ADMIN_SERVER_PORT=8020"});
    PL_CHECK_OK(zookeeper_run_result);

    // Run Kafka server.
    StatusOr<std::string> kafka_run_result = kafka_server_.Run(
        std::chrono::seconds{90},
        {absl::Substitute("--network=container:$0", zookeeper_server_.container_name()),
         "--name=kafka", "--env=KAFKA_ZOOKEEPER_CONNECT=localhost:32181",
         "--env=KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://localhost:29092",
         "--env=KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR=1"});
    PL_CHECK_OK(zookeeper_run_result);
  }

  StatusOr<int32_t> GetPIDFromOutput(std::string_view out) {
    std::vector<std::string_view> lines = absl::StrSplit(out, "\n");
    if (lines.empty()) {
      return error::Internal("Executed output (pid) from command.");
    }

    int32_t client_pid;
    if (!absl::SimpleAtoi(lines[0], &client_pid)) {
      return error::Internal("Could not extract PID.");
    }

    return client_pid;
  }

  StatusOr<int32_t> CreateTopic() {
    std::string cmd = absl::StrFormat(
        "docker exec %s bash -c 'kafka-topics --create --topic foo --partitions 1 "
        "--replication-factor 1 --if-not-exists --zookeeper localhost:32181 & echo $! && wait'",
        kafka_server_.container_name());

    PL_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  StatusOr<int32_t> ProduceMessage() {
    std::string cmd = absl::StrFormat(
        "docker exec %s bash -c 'echo \"hello\" | "
        "kafka-console-producer --request-required-acks 1 --broker-list localhost:29092 --topic "
        "foo& echo $! && wait'",
        kafka_server_.container_name());

    PL_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  KafkaContainer kafka_server_;
  ZooKeeperContainer zookeeper_server_;
};

struct KafkaTraceRecord {
  int64_t ts_ns = 0;
  kafka::APIKey req_cmd;
  std::string req_body;
  std::string resp;

  std::string ToString() const {
    return absl::Substitute("ts_ns=$0 req_cmd=$1 req_body=$2 resp=$3", ts_ns,
                            magic_enum::enum_name(req_cmd), req_body, resp);
  }
};

auto EqKafkaTraceRecord(const KafkaTraceRecord& x) {
  return AllOf(Field(&KafkaTraceRecord::req_cmd, Eq(x.req_cmd)),
               Field(&KafkaTraceRecord::req_body, StrEq(x.req_body)),
               Field(&KafkaTraceRecord::resp, StrEq(x.resp)));
}

KafkaTraceRecord kKafkaScriptCmd1 = {
    .req_cmd = kafka::APIKey::kProduce,
    .req_body =
        "{\"transactional_id\":\"\",\"acks\":1,\"timeout_ms\":1500,\"topics\":[{\"name\":\"foo\","
        "\"partitions\":[{\"index\":0,\"message_set\":{\"record_batches\":[{\"records\":[{\"key\":"
        ",\"value\":hello}]}]}}]}]}",
    .resp =
        "{\"topics\":[{\"name\":\"foo\",\"partitions\":[{\"index\":0,\"error_code\":\"kNone\","
        "\"record_errors\":[],\"error_message\":\"\"}]}],\"throttle_time_ms\":0}"};

std::vector<KafkaTraceRecord> GetKafkaTraceRecords(
    const types::ColumnWrapperRecordBatch& record_batch, int pid) {
  std::vector<KafkaTraceRecord> res;
  for (const auto& idx : FindRecordIdxMatchesPID(record_batch, kKafkaUPIDIdx, pid)) {
    res.push_back(KafkaTraceRecord{
        record_batch[kKafkaTimeIdx]->Get<types::Time64NSValue>(idx).val,
        static_cast<kafka::APIKey>(record_batch[kKafkaReqCmdIdx]->Get<types::Int64Value>(idx).val),
        std::string(record_batch[kKafkaReqBodyIdx]->Get<types::StringValue>(idx)),
        std::string(record_batch[kKafkaRespIdx]->Get<types::StringValue>(idx)),
    });
  }
  return res;
}

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(KafkaTraceTest, kafka_capture) {
  StartTransferDataThread();

  ASSERT_OK_AND_ASSIGN(int32_t create_topic_pid, CreateTopic());
  PL_UNUSED(create_topic_pid);
  ASSERT_OK_AND_ASSIGN(int32_t produce_message_pid, ProduceMessage());

  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kKafkaTableNum);
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  // TODO(chengruizhe): Some of the records are missing. Fix and add tests for all records.
  {
    auto records = GetKafkaTraceRecords(record_batch, kafka_server_.process_pid());
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kKafkaScriptCmd1)));
  }

  {
    auto records = GetKafkaTraceRecords(record_batch, produce_message_pid);
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kKafkaScriptCmd1)));
  }
}

}  // namespace stirling
}  // namespace px
