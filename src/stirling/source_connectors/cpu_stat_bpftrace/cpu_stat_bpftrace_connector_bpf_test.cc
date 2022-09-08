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

#include <string_view>

#include "src/common/testing/testing.h"

#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/cpu_stat_bpftrace/cpu_stat_bpftrace_connector.h"

namespace px {
namespace stirling {

class BPFTraceConnectorBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    source_ = CPUStatBPFTraceConnector::Create("cpu_stats");
    ASSERT_OK(source_->Init());

    ctx_ = std::make_unique<SystemWideStandaloneContext>();

    source_->InitContext(ctx_.get());

    // InitContext will cause Uprobes to deploy.
    // It won't return until the first set of uprobes has successfully deployed.
    // Sleep an additional second just to be safe.
    sleep(1);
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  const DataTableSchema& kTable = CPUStatBPFTraceConnector::kTable;
  const int kTableNum = 0;

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<SystemWideStandaloneContext> ctx_;
};

TEST_F(BPFTraceConnectorBPFTest, Basic) {
  sleep(5);

  DataTable data_table(/*id*/ 0, kTable);
  source_->set_data_tables({&data_table});
  source_->TransferData(ctx_.get());
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
}

}  // namespace stirling
}  // namespace px
