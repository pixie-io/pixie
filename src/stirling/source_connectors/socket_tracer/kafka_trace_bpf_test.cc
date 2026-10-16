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

#include <absl/strings/str_cat.h>
#include <absl/strings/str_replace.h>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/common/types.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/kafka_container.h"
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
using ::px::operator<<;

// Modern Kafka brokers (Kafka 3.1+) negotiate protocol versions well above what Pixie originally
// supported (e.g. Fetch v13+ with topic-id UUIDs, and higher Produce/ApiVersions/Metadata
// versions). This test runs against both the Confluent (confluentinc/cp-kafka) and Apache
// (apache/kafka) broker distributions in KRaft mode to confirm that Produce/Fetch/ApiVersions
// traces are captured for these modern versions, which regressed prior to this change (see
// https://github.com/pixie-io/pixie/issues/2138).
template <typename TKafkaContainer>
class KafkaTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
 protected:
  static constexpr std::string_view kTopic = "foo";
  static constexpr std::string_view kBootstrapServer = "localhost:29092";

  KafkaTraceTest() {
    // Run the Kafka broker in KRaft mode (no ZooKeeper). The env vars below are shared by both
    // the Confluent and Apache images since both translate KAFKA_* env vars into server config.
    StatusOr<std::string> kafka_run_result = kafka_server_.Run(
        std::chrono::seconds{120},
        {"--env=CLUSTER_ID=MkU3OEVBNTcwNTJENDM2Qk", "--env=KAFKA_NODE_ID=1",
         "--env=KAFKA_PROCESS_ROLES=broker,controller",
         "--env=KAFKA_CONTROLLER_QUORUM_VOTERS=1@localhost:29093",
         "--env=KAFKA_LISTENERS=PLAINTEXT://:29092,CONTROLLER://:29093",
         "--env=KAFKA_ADVERTISED_LISTENERS=PLAINTEXT://localhost:29092",
         "--env=KAFKA_CONTROLLER_LISTENER_NAMES=CONTROLLER",
         "--env=KAFKA_LISTENER_SECURITY_PROTOCOL_MAP=CONTROLLER:PLAINTEXT,PLAINTEXT:PLAINTEXT",
         "--env=KAFKA_OFFSETS_TOPIC_REPLICATION_FACTOR=1"});
    PX_CHECK_OK(kafka_run_result);
  }

  // Builds the path to a Kafka CLI tool for the broker distribution under test.
  std::string Tool(std::string_view name) {
    return absl::StrCat(TKafkaContainer::kBinPath, name, TKafkaContainer::kToolSuffix);
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
        "podman exec %s bash -c '%s --create --topic %s --partitions 1 "
        "--replication-factor 1 --if-not-exists --bootstrap-server %s & echo $! && wait'",
        kafka_server_.container_name(), Tool("kafka-topics"), kTopic, kBootstrapServer);

    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  StatusOr<int32_t> ProduceMessage() {
    std::string cmd = absl::StrFormat(
        "podman exec %s bash -c 'echo \"hello\" | "
        "%s --request-required-acks 1 --bootstrap-server %s --topic "
        "%s& echo $! && wait'",
        kafka_server_.container_name(), Tool("kafka-console-producer"), kBootstrapServer, kTopic);

    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  StatusOr<int32_t> FetchMessage() {
    std::string cmd = absl::StrFormat(
        "podman exec %s bash -c '%s --bootstrap-server %s --topic "
        "%s --from-beginning --timeout-ms 10000& echo $! && wait'",
        kafka_server_.container_name(), Tool("kafka-console-consumer"), kBootstrapServer, kTopic);

    PX_ASSIGN_OR_RETURN(std::string out, px::Exec(cmd));
    return GetPIDFromOutput(out);
  }

  TKafkaContainer kafka_server_;
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

// Matches a record by request command only.
auto EqKafkaCmd(kafka::APIKey req_cmd) {
  return Field(&KafkaTraceRecord::req_cmd, Eq(req_cmd));
}

// Matches a record by request command, with a substring expected in the request body.
auto EqKafkaCmdWithReqBody(kafka::APIKey req_cmd, std::string_view req_body_substr) {
  return AllOf(Field(&KafkaTraceRecord::req_cmd, Eq(req_cmd)),
               Field(&KafkaTraceRecord::req_body, HasSubstr(std::string(req_body_substr))));
}

// Matches a record by request command, with a substring expected in the response.
auto EqKafkaCmdWithResp(kafka::APIKey req_cmd, std::string_view resp_substr) {
  return AllOf(Field(&KafkaTraceRecord::req_cmd, Eq(req_cmd)),
               Field(&KafkaTraceRecord::resp, HasSubstr(std::string(resp_substr))));
}

using KafkaContainerTypes = ::testing::Types<::px::stirling::testing::KafkaContainer,
                                             ::px::stirling::testing::ApacheKafkaContainer>;
TYPED_TEST_SUITE(KafkaTraceTest, KafkaContainerTypes);

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TYPED_TEST(KafkaTraceTest, kafka_capture) {
  this->StartTransferDataThread();

  ASSERT_OK_AND_ASSIGN(int32_t create_topic_pid, this->CreateTopic());
  PX_UNUSED(create_topic_pid);
  ASSERT_OK_AND_ASSIGN(int32_t produce_message_pid, this->ProduceMessage());
  ASSERT_OK_AND_ASSIGN(int32_t fetch_message_pid, this->FetchMessage());

  this->StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets =
      this->ConsumeRecords(SocketTraceConnector::kKafkaTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  // Broker (server) side: it processes both the produce and fetch traffic. On modern brokers the
  // fetch uses topic-id UUIDs (api_version >= 13), which previously failed to parse.
  {
    auto records = GetKafkaTraceRecords(record_batch, this->kafka_server_.process_pid());
    EXPECT_THAT(records, Contains(EqKafkaCmdWithReqBody(kafka::APIKey::kProduce, "\"name\":\"foo\"")))
        << "Expected a Produce record referencing topic foo from the broker.";
    EXPECT_THAT(records, Contains(EqKafkaCmdWithResp(kafka::APIKey::kFetch, "message_set")))
        << "Expected a Fetch record with a decoded message_set from the broker.";
    EXPECT_THAT(records, Contains(EqKafkaCmd(kafka::APIKey::kFindCoordinator)));
    EXPECT_THAT(records, Contains(EqKafkaCmd(kafka::APIKey::kListOffsets)));
  }

  // Producer client: sends ApiVersions (negotiation) then Produce.
  {
    auto records = GetKafkaTraceRecords(record_batch, produce_message_pid);
    EXPECT_THAT(records, Contains(EqKafkaCmd(kafka::APIKey::kApiVersions)))
        << "Expected an ApiVersions record from the producer.";
    EXPECT_THAT(records, Contains(EqKafkaCmdWithReqBody(kafka::APIKey::kProduce, "\"name\":\"foo\"")))
        << "Expected a Produce record referencing topic foo from the producer.";
  }

  // Consumer client: sends ApiVersions then Fetch. The fetch request/response carry a topic_id
  // UUID (api_version >= 13). Capturing this record is the core regression fix.
  {
    auto records = GetKafkaTraceRecords(record_batch, fetch_message_pid);
    EXPECT_THAT(records, Contains(EqKafkaCmd(kafka::APIKey::kApiVersions)))
        << "Expected an ApiVersions record from the consumer.";
    EXPECT_THAT(records, Contains(EqKafkaCmdWithReqBody(kafka::APIKey::kFetch, "topic_id")))
        << "Expected a Fetch record with a decoded topic_id (UUID) from the consumer.";
    EXPECT_THAT(records, Contains(EqKafkaCmdWithResp(kafka::APIKey::kFetch, "message_set")))
        << "Expected a Fetch response with a decoded message_set from the consumer.";
  }
}

}  // namespace stirling
}  // namespace px
