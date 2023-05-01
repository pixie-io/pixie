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

#include "src/stirling/source_connectors/jvm_stats/jvm_stats_connector.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/source_connectors/jvm_stats/jvm_stats_table.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::AccessRecordBatch;
using ::px::stirling::testing::FindRecordsMatchingPID;
using ::px::stirling::testing::PIDToUPID;
using ::px::stirling::testing::RecordBatchSizeIs;
using ::px::testing::BazelRunfilePath;
using ::testing::Each;
using ::testing::SizeIs;

struct JavaHelloWorld : SubProcess {
  inline static const std::string javaBinPath =
      BazelRunfilePath("src/stirling/source_connectors/jvm_stats/testing/HelloWorld");

  ~JavaHelloWorld() {
    Kill();
    EXPECT_EQ(9, Wait());
  }

  Status Start() {
    auto status = SubProcess::Start({javaBinPath, "--jvm_flags=-Xms1m -Xmx4m", "HelloWorld"});
    // Sleep 2 seconds for the process to create the data file.
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return status;
  }
};

class JVMStatsConnectorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    connector_ = JVMStatsConnector::Create("jvm_stats_connector");
    ASSERT_OK(connector_->Init());
    connector_->set_data_tables(data_tables_.tables());
  }

  void TearDown() override { EXPECT_OK(connector_->Stop()); }

  std::unique_ptr<SourceConnector> connector_;
  DataTables data_tables_{JVMStatsConnector::kTables};
  DataTable* data_table_{data_tables_.tables().front()};
};

// NOTE: This test will likely break under --runs_per_tests=100 or higher because of limitations of
// Bazel's sandboxing.
//
// Bazel uses PID namespace, so the PID of the java subprocess is often the same in different test
// runs. However, Bazel does not uses chroot, or other mechanisms of isolating filesystems. So the
// Java subprocesses all writes to the same memory mapped file with the same path, which causes data
// corruption and test failures.
//
// Tests that java processes are detected and data is collected.
TEST_F(JVMStatsConnectorTest, CaptureData) {
  absl::flat_hash_set<md::UPID> upids;

  JavaHelloWorld hello_world1;
  ASSERT_OK(hello_world1.Start());
  upids.insert(PIDToUPID(hello_world1.child_pid()));

  {
    absl::flat_hash_set<md::UPID> upids = {PIDToUPID(hello_world1.child_pid())};
    auto ctx = std::make_unique<StandaloneContext>(upids);
    connector_->TransferData(ctx.get());
    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& records, tablets);

    ASSERT_THAT(records, RecordBatchSizeIs(1));
    EXPECT_GE(AccessRecordBatch<types::Int64Value>(records, kYoungGCTimeIdx, 0), 0);
    EXPECT_GE(AccessRecordBatch<types::Int64Value>(records, kFullGCTimeIdx, 0), 0);
    EXPECT_GE(AccessRecordBatch<types::Int64Value>(records, kUsedHeapSizeIdx, 0), 0);
    EXPECT_GE(AccessRecordBatch<types::Int64Value>(records, kTotalHeapSizeIdx, 0), 0);
    // This is derived from -Xmx4m. But we don't know how to control total_heap_size.
    EXPECT_GE(AccessRecordBatch<types::Int64Value>(records, kMaxHeapSizeIdx, 0), 4 * 1024 * 1024);
  }

  JavaHelloWorld hello_world2;
  ASSERT_OK(hello_world2.Start());
  upids.insert(PIDToUPID(hello_world2.child_pid()));

  {
    auto ctx = std::make_unique<StandaloneContext>(upids);
    connector_->TransferData(ctx.get());
    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& record_batch, tablets);
    const auto& records1 = FindRecordsMatchingPID(record_batch, kUPIDIdx, hello_world1.child_pid());
    const auto& records2 = FindRecordsMatchingPID(record_batch, kUPIDIdx, hello_world2.child_pid());

    EXPECT_THAT(records1, RecordBatchSizeIs(1));
    EXPECT_THAT(records2, RecordBatchSizeIs(1));
  }
}

}  // namespace stirling
}  // namespace px
