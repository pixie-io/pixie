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
#include <thread>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_websocket_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/go_websocket_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsSupersetOf;

class WebSocketTraceTest
    : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false> {
 protected:
  WebSocketTraceTest() {
    FLAGS_stirling_enable_websocket_tracing = TraceMode::On;
    PX_CHECK_OK(server_.Run(std::chrono::seconds{60}, {}));
  }

  ~WebSocketTraceTest() override { FLAGS_stirling_enable_websocket_tracing = TraceMode::Off; }

  ::px::stirling::testing::GoWebSocketServerContainer server_;
  ::px::stirling::testing::GoWebSocketClientContainer client_;
};

std::string Row(const types::ColumnWrapperRecordBatch& records, size_t i) {
  return absl::Substitute("encrypted=$0 direction=$1 opcode=$2 payload=$3",
                          records[kWebSocketEncryptedIdx]->Get<types::BoolValue>(i).val,
                          records[kWebSocketDirectionIdx]->Get<types::Int64Value>(i).val,
                          records[kWebSocketOpcodeIdx]->Get<types::Int64Value>(i).val,
                          records[kWebSocketPayloadIdx]->Get<types::StringValue>(i));
}

TEST_F(WebSocketTraceTest, PlainAndTLSEchoSessions) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_conn_trace_pid, server_.process_pid());
  StartTransferDataThread();

  PX_CHECK_OK(client_.Run(std::chrono::seconds{10},
                          {absl::Substitute("--network=container:$0", server_.container_name())},
                          {"--iters=2"}));
  client_.Wait();
  std::this_thread::sleep_for(5 * kTransferDataPeriod);

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> http_tablets = ConsumeRecords(SocketTraceConnector::kHTTPTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& http_batch, http_tablets);
  std::vector<std::string> upgrades;
  for (size_t i : FindRecordIdxMatchesPID(http_batch, kHTTPUPIDIdx, server_.process_pid())) {
    upgrades.push_back(
        absl::Substitute("$0 $1", http_batch[kHTTPReqPathIdx]->Get<types::StringValue>(i),
                         http_batch[kHTTPRespStatusIdx]->Get<types::Int64Value>(i).val));
  }
  ASSERT_THAT(upgrades, ElementsAre("/echo 101", "/echo 101"));

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kWebSocketTableNum);
  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);

  const std::vector<size_t> indices =
      FindRecordIdxMatchesPID(record_batch, kWebSocketUPIDIdx, server_.process_pid());
  std::vector<std::string> rows;
  std::vector<std::string> subprotocols;
  std::vector<std::string> req_paths;
  for (size_t i : indices) {
    rows.push_back(Row(record_batch, i));
    subprotocols.push_back(record_batch[kWebSocketSubprotocolIdx]->Get<types::StringValue>(i));
    req_paths.push_back(record_batch[kWebSocketReqPathIdx]->Get<types::StringValue>(i));
  }

  std::vector<std::string> expected;
  for (bool encrypted : {false, true}) {
    for (std::string_view payload : {"hello-0", "hello-1"}) {
      expected.push_back(
          absl::Substitute("encrypted=$0 direction=1 opcode=1 payload=$1", encrypted, payload));
      expected.push_back(
          absl::Substitute("encrypted=$0 direction=2 opcode=1 payload=$1", encrypted, payload));
    }
    expected.push_back(
        absl::Substitute("encrypted=$0 direction=1 opcode=2 payload=\\x01\\x02hi", encrypted));
  }
  EXPECT_THAT(rows, IsSupersetOf(expected));
  EXPECT_THAT(subprotocols, Each(std::string("echo.pixie")));
  EXPECT_THAT(req_paths, Each(std::string("/echo")));
}

}  // namespace stirling
}  // namespace px
