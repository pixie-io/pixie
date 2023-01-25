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

#include <absl/strings/str_split.h>

#include "src/common/testing/testing.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/types.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"
#include "src/stirling/stirling.h"

namespace px {
namespace stirling {

class SourceToTableTest : public ::testing::Test {
 protected:
  SourceToTableTest() : info_class_mgr_(SeqGenConnector::kSeq0Table) {}
  void SetUp() override {
    source_ = SeqGenConnector::Create("seq_gen");
    dynamic_cast<SeqGenConnector*>(source_.get())->ConfigureNumRowsPerGet(1);
    info_class_mgr_.SetSourceConnector(source_.get());
    table_ = std::make_unique<DataTable>(/*id*/ 0, info_class_mgr_.Schema());
  }

  std::unique_ptr<SourceConnector> source_;
  InfoClassManager info_class_mgr_;
  std::unique_ptr<DataTable> table_;
};

TEST_F(SourceToTableTest, source_to_table) {
  EXPECT_OK(source_->Init());
  std::vector<DataTable*> data_tables{table_.get(), nullptr};
  source_->set_data_tables(std::move(data_tables));
  SystemWideStandaloneContext ctx;
  source_->TransferData(&ctx);
  auto record_batches = table_->ConsumeRecords();
  for (const auto& record_batch : record_batches) {
    auto& columns = record_batch.records;

    ASSERT_EQ(types::DataType::TIME64NS, columns[0]->data_type());
    auto col0_val = columns[0]->Get<types::Time64NSValue>(0).val;
    PX_UNUSED(col0_val);

    ASSERT_EQ(types::DataType::INT64, columns[1]->data_type());
    auto col1_val = columns[1]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(1, col1_val);

    ASSERT_EQ(types::DataType::INT64, columns[2]->data_type());
    auto col2_val = columns[2]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(0, col2_val);

    ASSERT_EQ(types::DataType::INT64, columns[3]->data_type());
    auto col3_val = columns[3]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(0, col3_val);

    ASSERT_EQ(types::DataType::INT64, columns[4]->data_type());
    auto col4_val = columns[4]->Get<types::Int64Value>(0).val;
    EXPECT_EQ(1, col4_val);

    ASSERT_EQ(types::DataType::FLOAT64, columns[5]->data_type());
    auto col5_val = columns[5]->Get<types::Float64Value>(0).val;
    EXPECT_DOUBLE_EQ(0.0, col5_val);
  }
}

}  // namespace stirling
}  // namespace px
