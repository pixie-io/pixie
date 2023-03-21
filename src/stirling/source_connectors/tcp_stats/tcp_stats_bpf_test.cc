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
#include "src/common/testing/testing.h"
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
using ::testing::ContainsRegex;
using ::testing::HasSubstr;

class TcpTraceTest : public TcpTraceBPFTestFixture {};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(TcpTraceTest, Capture) {
  StartTransferDataThread();

  // Send "hello" to 127.0.0.1, total 6 bytes of data
  std::string cmd1 = "echo \"hello\" | nc 127.0.0.1 22";
  ASSERT_OK(px::Exec(cmd1));

  StopTransferDataThread();
  auto records = testing::ExtractToString(tcp_stats::kTCPStatsTable, tcp_stats_table_);

  EXPECT_THAT(records, HasSubstr("remote_addr:[127.0.0.1] remote_port:[22] tx:[6]"));

  StartTransferDataThread();

  // Send "This is sample test" to 127.0.0.1, total 20 bytes of data
  std::string cmd2 = "echo \"This is sample test\" | nc 127.0.0.1 22";
  ASSERT_OK(px::Exec(cmd2));

  StopTransferDataThread();
  auto records2 = testing::ExtractToString(tcp_stats::kTCPStatsTable, tcp_stats_table_);
  EXPECT_THAT(records2, HasSubstr("remote_addr:[127.0.0.1] remote_port:[22] tx:[20]"));

  // TODO: Explore options for testing retransmissions in a unit test case,
  // as retransmissions are blocking calls without known timeout value.
}

}  // namespace stirling
}  // namespace px
