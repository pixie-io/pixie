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

#include <regex>
#include <string>

#include <absl/strings/str_replace.h>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/kafka_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/zookeeper_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace kafka = protocols::kafka;

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::stirling::testing::SocketTraceBPFTestFixture;
using ::px::testing::BazelRunfilePath;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::StrEq;
using ::px::operator<<;

class KafkaTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  KafkaTraceTest() {
    // Run Zookeeper.
    StatusOr<std::string> zookeeper_run_result = zookeeper_server_.Run(
        std::chrono::seconds{90},
        {"--name=zookeeper", "--env=ZOOKEEPER_CLIENT_PORT=32181", "--env=ZOOKEEPER_TICK_TIME=2000",
         "--env=ZOOKEEPER_SYNC_LIMIT=2", "--env=ZOOKEEPER_ADMIN_SERVER_PORT=8020"});
    PX_CHECK_OK(zookeeper_run_result);

    // Run Kafka server.
    StatusOr<std::string> kafka_run_result = kafka_server_.Run(
        std::chrono::seconds{90},
        {absl::Substitute("--network=container:$0", zookeeper_server_.container_name()),
         "--name=kafka", "--env=KAFKA_ZOOKEEPER_CONNECT=localhost:32181",
         "--env=KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://localhost:29092",
         "--env=KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR=1"});
    PX_CHECK_OK(zookeeper_run_result);
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
        "podman exec %s bash -c 'kafka-topics --create --topic foo --partitions 1 "
        "--replication-factor 1 --if-not-exists --zookeeper localhost:32181 & echo $! && wait'",
        kafka_server_.container_name());

    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  StatusOr<int32_t> ProduceMessage() {
    std::string cmd = absl::StrFormat(
        "podman exec %s bash -c 'echo \"hello\" | "
        "kafka-console-producer --request-required-acks 1 --broker-list localhost:29092 --topic "
        "foo& echo $! && wait'",
        kafka_server_.container_name());

    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  StatusOr<int32_t> FetchMessage() {
    std::string cmd = absl::StrFormat(
        "podman exec %s bash -c 'kafka-console-consumer --bootstrap-server localhost:29092 --topic "
        "foo --from-beginning --timeout-ms 10000& echo $! && wait'",
        kafka_server_.container_name());

    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  ::px::stirling::testing::KafkaContainer kafka_server_;
  ::px::stirling::testing::ZooKeeperContainer zookeeper_server_;
};

struct KafkaTraceRecord {
  int64_t ts_ns = 0;
  kafka::APIKey req_cmd;
  std::string client_id;
  std::string req_body;
  std::string resp;

  std::string ToString() const {
    return absl::Substitute("ts_ns=$0 req_cmd=$1 client_id $2 req_body=$3 resp=$4", ts_ns,
                            magic_enum::enum_name(req_cmd), client_id, req_body, resp);
  }
};

auto EqKafkaTraceRecord(const KafkaTraceRecord& x) {
  return AllOf(Field(&KafkaTraceRecord::req_cmd, Eq(x.req_cmd)),
               // client_id is dynamic for the consumer.
               Field(&KafkaTraceRecord::client_id, HasSubstr(x.client_id)),
               Field(&KafkaTraceRecord::req_body, StrEq(x.req_body)),
               Field(&KafkaTraceRecord::resp, StrEq(x.resp)));
}

KafkaTraceRecord kLeaderAndIsrRecord = {
    .req_cmd = kafka::APIKey::kLeaderAndIsr, .client_id = "1001", .req_body = "", .resp = ""};

KafkaTraceRecord kUpdateMetadataRecord = {
    .req_cmd = kafka::APIKey::kUpdateMetadata, .client_id = "1001", .req_body = "", .resp = ""};

KafkaTraceRecord kProducerMetadataRecord = {.req_cmd = kafka::APIKey::kMetadata,
                                            .client_id = "console-producer",
                                            .req_body = "",
                                            .resp = ""};

KafkaTraceRecord kConsumerMetadataRecord = {.req_cmd = kafka::APIKey::kMetadata,
                                            .client_id = "console-consumer",
                                            .req_body = "",
                                            .resp = ""};

KafkaTraceRecord kFindCoordinatorRecord = {.req_cmd = kafka::APIKey::kFindCoordinator,
                                           .client_id = "console-consumer",
                                           .req_body = "",
                                           .resp = ""};

KafkaTraceRecord kProducerApiVersionsRecord = {.req_cmd = kafka::APIKey::kApiVersions,
                                               .client_id = "console-producer",
                                               .req_body = "",
                                               .resp = ""};

KafkaTraceRecord kConsumerApiVersionsRecord = {.req_cmd = kafka::APIKey::kApiVersions,
                                               .client_id = "console-consumer",
                                               .req_body = "",
                                               .resp = ""};

KafkaTraceRecord kJoinGroupRecord = {
    .req_cmd = kafka::APIKey::kJoinGroup,
    .client_id = "console-consumer",
    .req_body =
        "{\"group_id\":\"console-consumer\",\"session_timeout_ms\":10000,\"rebalance_timeout_ms\":"
        "300000,\"member_id\":\"consumer-console-consumer\",\"group_instance_id\":\"\",\"protocol_"
        "type\":\"consumer\",\"protocols\":[{\"protocol\":\"range\"}]}",
    .resp =
        "{\"throttle_time_ms\":0,\"error_code\":0,\"generation_id\":1,\"protocol_type\":"
        "\"consumer\",\"protocol_name\":\"range\",\"leader\":\"consumer-console-consumer\","
        "\"member_id\":\"consumer-console-consumer\",\"members\":[{\"member_id\":\"consumer-"
        "console-consumer\",\"group_instance_id\":\"\"}]}"};

KafkaTraceRecord kSyncGroupRecord = {
    .req_cmd = kafka::APIKey::kSyncGroup,
    .client_id = "console-consumer",
    .req_body =
        "{\"group_id\":\"console-consumer\",\"generation_id\":1,\"member_id\":\"consumer-console-"
        "consumer\",\"group_instance_id\":\"\",\"protocol_type\":\"consumer\",\"protocol_name\":"
        "\"range\",\"assignments\":[{\"member_id\":\"consumer-console-consumer\"}]}",
    .resp =
        "{\"throttle_time_ms\":0,\"error_code\":0,\"protocol_type\":\"consumer\",\"protocol_name\":"
        "\"range\"}"};

KafkaTraceRecord kProduceRecord = {
    .req_cmd = kafka::APIKey::kProduce,
    .client_id = "console-producer",
    .req_body =
        "{\"transactional_id\":\"\",\"acks\":1,\"timeout_ms\":1500,\"topics\":[{\"name\":\"foo\","
        "\"partitions\":[{\"index\":0,\"message_set\":{\"size\":74}}]}]}",
    .resp =
        "{\"topics\":[{\"name\":\"foo\",\"partitions\":[{\"index\":0,\"error_code\":0,"
        "\"base_offset\":0,\"log_append_time_ms\":-1,\"log_start_offset\":0,\"record_errors\":[],"
        "\"error_message\":\"\"}]}],\"throttle_time_ms\":0}"};

KafkaTraceRecord kOffsetFetchRecord = {.req_cmd = kafka::APIKey::kOffsetFetch,
                                       .client_id = "console-consumer",
                                       .req_body = "",
                                       .resp = ""};

KafkaTraceRecord kListOffsetsRecord = {.req_cmd = kafka::APIKey::kListOffsets,
                                       .client_id = "console-consumer",
                                       .req_body = "",
                                       .resp = ""};

KafkaTraceRecord kFetchRecord = {
    .req_cmd = kafka::APIKey::kFetch,
    .client_id = "console-consumer",
    .req_body =
        "{\"replica_id\":-1,\"session_id\":0,\"session_epoch\":0,\"topics\":[{\"name\":\"foo\","
        "\"partitions\":[{\"index\":0,\"current_leader_epoch\":0,\"fetch_offset\":0,\"last_fetched_"
        "epoch\":-1,\"log_start_offset\":-1,\"partition_max_bytes\":1048576}]}],\"forgotten_"
        "topics\":[],\"rack_id\":\"\"}",
    .resp =
        "{\"throttle_time_ms\":0,\"error_code\":0,\"session_id\":<removed>,\"topics\":[{\"name\":"
        "\"foo\",\"partitions\":[{"
        "\"index\":0,\"error_code\":0,\"high_"
        "watermark\":1,\"last_stable_offset\":1,\"log_start_offset\":0,\"aborted_transactions\":[],"
        "\"preferred_read_replica\":-1,\"message_set\":{\"size\":74}}]}]}"};

std::vector<KafkaTraceRecord> GetKafkaTraceRecords(
    const types::ColumnWrapperRecordBatch& record_batch, int pid) {
  std::vector<KafkaTraceRecord> res;
  for (const auto& idx : FindRecordIdxMatchesPID(record_batch, kKafkaUPIDIdx, pid)) {
    std::string resp = std::string(record_batch[kKafkaRespIdx]->Get<types::StringValue>(idx));
    std::string req = std::string(record_batch[kKafkaReqBodyIdx]->Get<types::StringValue>(idx));

    // Masking the session_id in the response, because it is dynamic.
    std::regex session_id_re(",\"session_id\":\\d+");
    resp = std::regex_replace(resp, session_id_re, ",\"session_id\":<removed>");

    // Masking the consumer-consumer-* in the request and response, because it is dynamic.
    std::regex client_id_re("console-consumer[a-z0-9-]+");
    req = std::regex_replace(req, client_id_re, "console-consumer");
    resp = std::regex_replace(resp, client_id_re, "console-consumer");
    res.push_back(KafkaTraceRecord{
        record_batch[kKafkaTimeIdx]->Get<types::Time64NSValue>(idx).val,
        static_cast<kafka::APIKey>(record_batch[kKafkaReqCmdIdx]->Get<types::Int64Value>(idx).val),
        std::string(record_batch[kKafkaClientIDIdx]->Get<types::StringValue>(idx)), req, resp});
  }
  return res;
}

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(KafkaTraceTest, kafka_capture) {
  StartTransferDataThread();

  ASSERT_OK_AND_ASSIGN(int32_t create_topic_pid, CreateTopic());
  PX_UNUSED(create_topic_pid);
  ASSERT_OK_AND_ASSIGN(int32_t produce_message_pid, ProduceMessage());
  ASSERT_OK_AND_ASSIGN(int32_t fetch_message_pid, FetchMessage());

  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kKafkaTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
  // TODO(chengruizhe): Some of the records are missing. Fix and add tests for all records.
  // TODO(vsrivatsa): enable kafka Metadata test once Metadata response parsing implemeneted
  {
    auto records = GetKafkaTraceRecords(record_batch, kafka_server_.process_pid());
    // ApiVersion requests are dropped by the server, since they are the first packets.
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kLeaderAndIsrRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kUpdateMetadataRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kProduceRecord)));
    // EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kConsumerMetadataRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kFindCoordinatorRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kJoinGroupRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kSyncGroupRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kOffsetFetchRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kListOffsetsRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kFetchRecord)));
  }
  {
    auto records = GetKafkaTraceRecords(record_batch, produce_message_pid);
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kProducerApiVersionsRecord)));
    // EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kProducerMetadataRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kProduceRecord)));
  }
  {
    auto records = GetKafkaTraceRecords(record_batch, fetch_message_pid);
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kConsumerApiVersionsRecord)));
    // EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kConsumerMetadataRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kFindCoordinatorRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kJoinGroupRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kSyncGroupRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kOffsetFetchRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kListOffsetsRecord)));
    EXPECT_THAT(records, Contains(EqKafkaTraceRecord(kFetchRecord)));
  }
}

}  // namespace stirling
}  // namespace px
