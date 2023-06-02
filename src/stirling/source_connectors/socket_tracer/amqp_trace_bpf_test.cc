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
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/rabbitmq_consumer_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/rabbitmq_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/rabbitmq_producer_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

#include "src/stirling/source_connectors/socket_tracer/protocols/amqp/types_gen.h"
namespace px {
namespace stirling {

namespace amqp = protocols::amqp;

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

class AMQPTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
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

  void RunAll() {
    rabbitmq_server_.Run(std::chrono::seconds{120});
    rabbitmq_consumer_.Run(
        std::chrono::seconds{120},
        {absl::Substitute("--network=container:$0", rabbitmq_server_.container_name())});

    rabbitmq_producer_.Run(
        std::chrono::seconds{120},
        {absl::Substitute("--network=container:$0", rabbitmq_server_.container_name())});
  }

  ::px::stirling::testing::RabbitMQConsumer rabbitmq_consumer_;
  ::px::stirling::testing::RabbitMQProducer rabbitmq_producer_;
  ::px::stirling::testing::RabbitMQContainer rabbitmq_server_;
};

struct AMQPTraceRecord {
  uint8_t frame_type;
  uint8_t channel_id;
  uint8_t req_class_id;
  uint8_t req_method_id;
  uint8_t resp_class_id;
  uint8_t resp_method_id;

  std::string ToString() const {
    return absl::Substitute(
        "frame_type=$0 channel_id=$1 req_class_id=$2 resp_class_id=$3 resp_method_id=$4 "
        "resp_method_id=$5",
        frame_type, channel_id, req_class_id, req_method_id, resp_class_id, resp_method_id);
  }
};

auto EqAMQPTraceRecord(const AMQPTraceRecord& x) {
  return AllOf(Field(&AMQPTraceRecord::frame_type, Eq(x.frame_type)),
               Field(&AMQPTraceRecord::channel_id, Eq(x.channel_id)),
               Field(&AMQPTraceRecord::req_class_id, Eq(x.req_class_id)),
               Field(&AMQPTraceRecord::req_method_id, Eq(x.req_method_id)),
               Field(&AMQPTraceRecord::resp_class_id, Eq(x.resp_class_id)),
               Field(&AMQPTraceRecord::resp_method_id, Eq(x.resp_method_id)));
}

AMQPTraceRecord kConnectStartRecord = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 0,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kConnection),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPConnectionMethods::kAMQPConnectionStartOk),
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kConnection),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPConnectionMethods::kAMQPConnectionStart),
};

AMQPTraceRecord kConnectTuneRecord = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 0,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kConnection),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPConnectionMethods::kAMQPConnectionTuneOk),
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kConnection),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPConnectionMethods::kAMQPConnectionTune),
};

AMQPTraceRecord kConnectOpen = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 0,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kConnection),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPConnectionMethods::kAMQPConnectionOpen),
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kConnection),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPConnectionMethods::kAMQPConnectionOpenOk),
};

AMQPTraceRecord kChannelOpen = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 1,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kChannel),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPChannelMethods::kAMQPChannelOpen),
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kChannel),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPChannelMethods::kAMQPChannelOpenOk),
};

AMQPTraceRecord kQueueDeclare = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 1,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kQueue),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPQueueMethods::kAMQPQueueDeclare),
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kQueue),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPQueueMethods::kAMQPQueueDeclareOk),
};

AMQPTraceRecord kBasicConsume = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 1,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kBasic),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPBasicMethods::kAMQPBasicConsume),
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kBasic),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPBasicMethods::kAMQPBasicConsumeOk),
};

AMQPTraceRecord kBasicPublish = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 1,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kBasic),
    .req_method_id = static_cast<uint8_t>(amqp::AMQPBasicMethods::kAMQPBasicPublish),
    .resp_class_id = 0,
    .resp_method_id = 0};

AMQPTraceRecord kBasicContentHeader = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameHeader),
    .channel_id = 1,
    .req_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kBasic),
    .req_method_id = 0,
    .resp_class_id = 0,
    .resp_method_id = 0};

AMQPTraceRecord kContentBody = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameBody),
    .channel_id = 1,
    .req_class_id = 0,
    .req_method_id = 0,
    .resp_class_id = 0,
    .resp_method_id = 0};

AMQPTraceRecord kBasicRespContentHeader = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameHeader),
    .channel_id = 0,
    .req_class_id = 0,
    .req_method_id = 0,
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kBasic),
    .resp_method_id = 0};
AMQPTraceRecord kBasicDeliver = {
    .frame_type = static_cast<uint8_t>(amqp::AMQPFrameTypes::kFrameMethod),
    .channel_id = 0,
    .req_class_id = 0,
    .req_method_id = 0,
    .resp_class_id = static_cast<uint8_t>(amqp::AMQPClasses::kBasic),
    .resp_method_id = static_cast<uint8_t>(amqp::AMQPBasicMethods::kAMQPBasicDeliver)};

std::vector<AMQPTraceRecord> GetAMQPTraceRecords(
    const types::ColumnWrapperRecordBatch& record_batch) {
  std::vector<AMQPTraceRecord> res;
  for (const types::SharedColumnWrapper& column_wrapper : record_batch) {
    for (size_t record_idx = 0; record_idx < column_wrapper->Size(); ++record_idx) {
      uint8_t frame_type = static_cast<uint8_t>(
          record_batch[kAMQPFrameTypeIdx]->Get<types::Int64Value>(record_idx).val);
      uint8_t channel_id = static_cast<uint8_t>(
          record_batch[kAMQPChannelIdx]->Get<types::Int64Value>(record_idx).val);
      uint8_t class_id = static_cast<uint8_t>(
          record_batch[kAMQPReqClassIdx]->Get<types::Int64Value>(record_idx).val);
      uint8_t method_id = static_cast<uint8_t>(
          record_batch[kAMQPReqMethodIdx]->Get<types::Int64Value>(record_idx).val);

      uint8_t resp_class_id = static_cast<uint8_t>(
          record_batch[kAMQPRespClassIdx]->Get<types::Int64Value>(record_idx).val);
      uint8_t resp_method_id = static_cast<uint8_t>(
          record_batch[kAMQPRespMethodIdx]->Get<types::Int64Value>(record_idx).val);
      res.push_back(AMQPTraceRecord{
          .frame_type = frame_type,
          .channel_id = channel_id,
          .req_class_id = class_id,
          .req_method_id = method_id,
          .resp_class_id = resp_class_id,
          .resp_method_id = resp_method_id,
      });
    }
  }
  return res;
}
//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(AMQPTraceTest, AMQPCapture) {
  StartTransferDataThread();
  RunAll();
  StopTransferDataThread();

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kAMQPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  {
    std::vector<AMQPTraceRecord> records = GetAMQPTraceRecords(record_batch);
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kConnectStartRecord)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kConnectTuneRecord)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kConnectOpen)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kChannelOpen)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kQueueDeclare)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kBasicConsume)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kBasicPublish)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kBasicContentHeader)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kContentBody)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kBasicRespContentHeader)));
    EXPECT_THAT(records, Contains(EqAMQPTraceRecord(kBasicDeliver)));
  }
}

}  // namespace stirling
}  // namespace px
