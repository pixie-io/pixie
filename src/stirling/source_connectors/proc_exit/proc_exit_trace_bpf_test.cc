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

#include "src/stirling/source_connectors/proc_exit/proc_exit_connector.h"

#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_tables.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {
namespace proc_exit_tracer {

using ::px::SubProcess;
using ::px::stirling::testing::RecordBatchSizeIs;
using ::px::testing::BazelRunfilePath;
using ::testing::SizeIs;

// Tests that ProcExitConnector::TransferData() does not throw any failures.
TEST(ProcExitConnectorTest, TransferData) {
  auto connector = ProcExitConnector::Create("test_proc_exit_connector");
  ASSERT_TRUE(connector != nullptr);
  EXPECT_OK(connector->Init());
  StandaloneContext context({md::UPID{0, 1, 1}});

  DataTables data_tables{ProcExitConnector::kTables};
  DataTable* data_table = data_tables.tables().front();

  connector->set_data_tables(data_tables.tables());

  const std::filesystem::path sleep_path =
      BazelRunfilePath("src/stirling/source_connectors/proc_exit/testing/sleep");
  SubProcess proc;
  ASSERT_OK(proc.Start({sleep_path.string()}));
  ASSERT_TRUE(proc.IsRunning());
  proc.Kill();
  int retval = proc.Wait();
  EXPECT_EQ(retval, 9) << "Exit should not be 0 when killed";
  connector->TransferData(&context);

  types::ColumnWrapperRecordBatch result = testing::ExtractRecordsMatchingPID(
      data_table, kProcExitEventsTable.ColIndex("upid"), proc.child_pid());
  ASSERT_THAT(result, RecordBatchSizeIs(1));
  EXPECT_EQ(result[proc_exit_tracer::kSignalIdx]->Get<types::Int64Value>(0), 9);
  // Process abnormally terminated will not have a meaningful exit code.
  // So here 0 means it's not set, instead of that it succeeded.
  EXPECT_EQ(result[proc_exit_tracer::kExitCodeIdx]->Get<types::Int64Value>(0), 0);
  EXPECT_EQ(result[proc_exit_tracer::kCommIdx]->Get<types::StringValue>(0), "sleep");
}

}  // namespace proc_exit_tracer
}  // namespace stirling
}  // namespace px
