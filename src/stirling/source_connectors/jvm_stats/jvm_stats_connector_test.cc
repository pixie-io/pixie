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

#include "src/common/base/test_utils.h"
#include "src/common/exec/subprocess.h"
#include "src/common/testing/test_environment.h"
#include "src/stirling/source_connectors/jvm_stats/jvm_stats_table.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::ColWrapperSizeIs;
using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::testing::TestFilePath;
using ::testing::Each;
using ::testing::SizeIs;

struct JavaHelloWorld : SubProcess {
  inline static const std::string kClassPath =
      TestFilePath("src/stirling/source_connectors/jvm_stats/testing/HelloWorld.jar");

  ~JavaHelloWorld() {
    Kill();
    EXPECT_EQ(9, Wait());
  }

  Status Start() {
    auto status = SubProcess::Start({"java", "-cp", kClassPath, "-Xms1m", "-Xmx4m", "HelloWorld"});
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
  }

  void TearDown() override { EXPECT_OK(connector_->Stop()); }

  std::unique_ptr<SourceConnector> connector_;
  DataTable data_table_{/*id*/ 0, kJVMStatsTable};
  const std::vector<DataTable*> data_tables_{&data_table_};
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
  std::unique_ptr<StandaloneContext> ctx;
  JavaHelloWorld hello_world1;
  ASSERT_OK(hello_world1.Start());

  std::vector<TaggedRecordBatch> tablets;
  types::ColumnWrapperRecordBatch record_batch;

  ctx = std::make_unique<StandaloneContext>();
  connector_->TransferData(ctx.get(), data_tables_);
  tablets = data_table_.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  auto idxes = FindRecordIdxMatchesPID(record_batch, kUPIDIdx, hello_world1.child_pid());
  ASSERT_THAT(idxes, SizeIs(1));

  auto idx = idxes[0];

  md::UPID upid(record_batch[kUPIDIdx]->Get<types::UInt128Value>(idx).val);
  std::filesystem::path proc_pid_path =
      system::Config::GetInstance().proc_path() / std::to_string(hello_world1.child_pid());
  ASSERT_OK_AND_ASSIGN(int64_t pid_start_time, system::GetPIDStartTimeTicks(proc_pid_path));
  md::UPID expected_upid(/* asid */ 0, hello_world1.child_pid(), pid_start_time);
  EXPECT_EQ(upid, expected_upid);

  EXPECT_GE(record_batch[kYoungGCTimeIdx]->Get<types::Int64Value>(idx), 0);
  EXPECT_GE(record_batch[kFullGCTimeIdx]->Get<types::Int64Value>(idx), 0);
  EXPECT_GE(record_batch[kUsedHeapSizeIdx]->Get<types::Int64Value>(idx).val, 0);
  EXPECT_GE(record_batch[kTotalHeapSizeIdx]->Get<types::Int64Value>(idx).val, 0);
  // This is derived from -Xmx4m. But we don't know how to control total_heap_size.
  EXPECT_GE(record_batch[kMaxHeapSizeIdx]->Get<types::Int64Value>(idx).val, 4 * 1024 * 1024);

  JavaHelloWorld hello_world2;
  ASSERT_OK(hello_world2.Start());
  std::this_thread::sleep_for(std::chrono::seconds(2));

  ctx = std::make_unique<StandaloneContext>();
  connector_->TransferData(ctx.get(), data_tables_);
  tablets = data_table_.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  record_batch = tablets[0].records;
  EXPECT_THAT(FindRecordIdxMatchesPID(record_batch, kUPIDIdx, hello_world2.child_pid()), SizeIs(1));
  // Make sure the previous processes were scanned as well.
  EXPECT_THAT(FindRecordIdxMatchesPID(record_batch, kUPIDIdx, hello_world1.child_pid()), SizeIs(1));
}

}  // namespace stirling
}  // namespace px
