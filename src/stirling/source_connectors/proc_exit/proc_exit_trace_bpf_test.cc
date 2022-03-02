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

#include "src/common/testing/testing.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::testing::SizeIs;

// Tests that ProcExitConnector::TransferData() does not throw any failures.
TEST(ProcExitConnectorTest, TransferData) {
  auto connector = ProcExitConnector::Create("test_proc_exit_connector");
  ASSERT_TRUE(connector != nullptr);
  EXPECT_OK(connector->Init());
  TestContext context({md::UPID{0, 1, 1}});

  testing::DataTables data_tables{ProcExitConnector::kTables};
  DataTable* data_table = data_tables.tables().front();

  // TODO(yzhao): Add a test program and kill it, and verify the record.
  connector->TransferData(&context, data_tables.tables());
  PL_LOG_VAR(testing::ExtractToString(ProcExitConnector::kTable, data_table));
}

}  // namespace stirling
}  // namespace px
