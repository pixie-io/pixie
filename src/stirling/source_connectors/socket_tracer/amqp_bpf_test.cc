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
#include "src/stirling/source_connectors/socket_tracer/testing/container_images.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

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

class AMQPTraceTest : public SocketTraceBPFTestFixture</* TClientSideTracing */ true> {
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
  int64_t ts_ns = 0;
  std::string frame_type;
  std::string class_id;
  std::string method_id;
  std::string channel;

  std::string ToString() const {
    return absl::Substitute("ts_ns=$0 frame_type=$1 class_id=$2 method_id=$3 channel=$4", ts_ns,
                            frame_type, class_id, method_id, channel);
  }
};

TEST_F(AMQPTraceTest, AMQPCapture) {
  StartTransferDataThread();
  RunAll();
  StopTransferDataThread();
}

}  // namespace stirling
}  // namespace px
