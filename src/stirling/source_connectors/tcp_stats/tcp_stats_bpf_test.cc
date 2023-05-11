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

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/tcp_stats/tcp_stats_connector.h"
#include "src/stirling/source_connectors/tcp_stats/tcp_stats_table.h"
#include "src/stirling/source_connectors/tcp_stats/testing/tcp_stats_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::RecordBatchSizeIs;
using ::px::stirling::testing::TcpTraceBPFTestFixture;
using ::px::stirling::testing::WaitAndExpectRecords;
using ::px::types::Int64Value;
using ::px::types::StringValue;
using tcp_stats::kTCPBytesReceivedIdx;
using tcp_stats::kTCPBytesSentIdx;
using tcp_stats::kTCPLocalAddrIdx;
using tcp_stats::kTCPLocalPortIdx;
using tcp_stats::kTCPRemoteAddrIdx;
using tcp_stats::kTCPRemotePortIdx;
using tcp_stats::kTCPRetransmitsIdx;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;

class TcpTraceTest : public TcpTraceBPFTestFixture {};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

std::vector<TcpStatsRecord> ToTcpStatsRecordVector(
    const std::vector<TaggedRecordBatch>& record_batches) {
  std::vector<TcpStatsRecord> result;

  for (size_t i = 0; i < record_batches.size(); i++) {
    auto record_batch = record_batches[i].records;
    for (const types::SharedColumnWrapper& column_wrapper : record_batch) {
      for (size_t idx = 0; idx < column_wrapper->Size(); ++idx) {
        TcpStatsRecord r;
        r.local_addr = record_batch[kTCPLocalAddrIdx]->Get<StringValue>(idx);
        r.local_port = record_batch[kTCPLocalPortIdx]->Get<Int64Value>(idx).val;
        r.remote_addr = record_batch[kTCPRemoteAddrIdx]->Get<StringValue>(idx);
        r.remote_port = record_batch[kTCPRemotePortIdx]->Get<Int64Value>(idx).val;
        r.tx = record_batch[kTCPBytesSentIdx]->Get<Int64Value>(idx).val;
        r.rx = record_batch[kTCPBytesReceivedIdx]->Get<Int64Value>(idx).val;
        r.retransmits = record_batch[kTCPRetransmitsIdx]->Get<Int64Value>(idx).val;
        result.push_back(r);
      }
    }
  }

  return result;
}

TEST_F(TcpTraceTest, Capture) {
  SubProcess server_proc;
  std::thread server_thread([&] {
    std::vector<std::string> args = {
        "nc", "-l", "-p", "12345", "-s", "127.0.0.1", "-v",
    };
    ASSERT_OK(server_proc.Start(args, /* stderr_to_stdout */ true));
  });
  server_thread.detach();

  StartTransferDataThread();

  // Send "hello" to 127.0.0.1, total 6 bytes of data
  std::string cmd1 = "echo \"hello\" | nc -w1 127.0.0.1 12345 -v";
  ASSERT_OK(px::Exec(cmd1));

  StopTransferDataThread();

  std::vector<TcpStatsRecord> expected = {
      {
          .local_addr = "",
          .local_port = 0,
          .remote_addr = "127.0.0.1",
          .remote_port = 12345,
          .tx = 6,
          .rx = 0,
          .retransmits = 0,
      },
  };

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(TCPStatsConnector::kTCPStatsTableNum);
  testing::Timeout t(std::chrono::minutes{1});
  auto records = ToTcpStatsRecordVector(tablets);
  while (!testing::RecordsContains(records, expected) && !t.TimedOut()) {
    auto new_records = ToTcpStatsRecordVector(ConsumeRecords(TCPStatsConnector::kTCPStatsTableNum));
    records.insert(records.end(), new_records.begin(), new_records.end());
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
  }

  EXPECT_THAT(records, IsSupersetOf(expected));
  // TODO(RagalahariP): Explore options for testing retransmissions in a unit test case,
  // as retransmissions are blocking calls without known timeout value.
}

}  // namespace stirling
}  // namespace px
