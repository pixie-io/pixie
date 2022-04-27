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

#include <absl/functional/bind_front.h>

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/output.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"
#include "src/stirling/source_connectors/stirling_error/stirling_error_connector.h"
#include "src/stirling/source_connectors/stirling_error/stirling_error_table.h"
#include "src/stirling/stirling.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::testing::SizeIs;

using ::px::types::ColumnWrapperRecordBatch;
using ::px::types::Int64Value;
using ::px::types::StringValue;
using ::px::types::Time64NSValue;
using ::px::types::UInt128Value;

using ::px::stirling::testing::RecordBatchSizeIs;

// A SourceConnector that fails on Init.
class FaultyConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "faulty connector";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{500};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  // clang-format off
  static constexpr DataElement kElements[] = {
    {"time_",
      "Timestamp when the data record was collected.",
      types::DataType::TIME64NS,
      types::SemanticType::ST_NONE,
      types::PatternType::METRIC_COUNTER},
  };
  // clang-format on
  static constexpr auto kTable0 = DataTableSchema("table0", "A test table.", kElements);

  static constexpr auto kTables = MakeArray(kTable0);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new FaultyConnector(name));
  }

 protected:
  explicit FaultyConnector(std::string_view name) : SourceConnector(name, kTables) {}
  ~FaultyConnector() override = default;

  Status InitImpl() override {
    sampling_freq_mgr_.set_period(kSamplingPeriod);
    push_freq_mgr_.set_period(kPushPeriod);
    return error::Internal("Initialization failed on purpose.");
  };

  void TransferDataImpl(ConnectorContext* /* ctx */,
                        const std::vector<DataTable*>& /* data_tables */) override{};

  Status StopImpl() override { return Status::OK(); }
};

class StirlingErrorTest : public ::testing::Test {
 protected:
  inline static const uint32_t kNumSources = 3;

  template <typename TSourceType>
  std::unique_ptr<SourceRegistry> CreateSources() {
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    for (uint32_t i = 0; i < kNumSources; ++i) {
      registry->RegisterOrDie<TSourceType>(absl::Substitute("sequences$0", i));
    }
    registry->RegisterOrDie<StirlingErrorConnector>("stirling_error");
    return registry;
  }

  void InitStirling(std::unique_ptr<SourceRegistry> registry) {
    stirling_ = Stirling::Create(std::move(registry));
    stirling_->RegisterDataPushCallback(absl::bind_front(&StirlingErrorTest::AppendData, this));

    stirlingpb::Publish publication;
    stirling_->GetPublishProto(&publication);
    IndexPublication(publication, &table_info_map_);
  }

  Status AppendData(uint64_t table_id, types::TabletID tablet_id,
                    std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
    PL_UNUSED(tablet_id);

    // Append stirling error table records to record_batches.
    auto iter = table_info_map_.find(table_id);
    if (iter != table_info_map_.end()) {
      const stirlingpb::InfoClass& table_info = iter->second;
      if (table_info.schema().name() == "stirling_error") {
        record_batches_.push_back(std::move(record_batch));
      }
    }
    return Status::OK();
  }

  absl::flat_hash_map<uint64_t, stirlingpb::InfoClass> table_info_map_;
  std::unique_ptr<Stirling> stirling_;
  std::vector<std::unique_ptr<ColumnWrapperRecordBatch>> record_batches_;
};

TEST_F(StirlingErrorTest, NoError) {
  auto registry = CreateSources<SeqGenConnector>();
  InitStirling(std::move(registry));

  ASSERT_OK(stirling_->RunAsThread());
  ASSERT_OK(stirling_->WaitUntilRunning(std::chrono::seconds(5)));
  sleep(5);
  stirling_->Stop();

  EXPECT_THAT(record_batches_, SizeIs(1));
  auto& record_batch = *record_batches_[0];
  // Stirling Error Source Connector plus the other ones.
  EXPECT_THAT(record_batch, RecordBatchSizeIs(kNumSources + 1));

  for (size_t i = 0; i < kNumSources + 1; ++i) {
    std::string expected_source =
        i < kNumSources ? absl::Substitute("sequences$0", i) : "stirling_error";
    EXPECT_EQ(record_batch[2]->Get<StringValue>(i).string(), expected_source);
    EXPECT_EQ(record_batch[3]->Get<Int64Value>(i).val, px::statuspb::Code::OK);
    EXPECT_EQ(record_batch[4]->Get<StringValue>(i).string(), "");
  }
}

TEST_F(StirlingErrorTest, InitializationError) {
  auto registry = CreateSources<FaultyConnector>();
  InitStirling(std::move(registry));

  ASSERT_OK(stirling_->RunAsThread());
  ASSERT_OK(stirling_->WaitUntilRunning(std::chrono::seconds(5)));
  sleep(5);
  stirling_->Stop();

  EXPECT_THAT(record_batches_, SizeIs(1));
  auto& record_batch = *record_batches_[0];
  // Stirling Error Source Connector plus the other ones.
  EXPECT_THAT(record_batch, RecordBatchSizeIs(kNumSources + 1));

  for (size_t i = 0; i < kNumSources + 1; ++i) {
    std::string expected_source =
        i < kNumSources ? absl::Substitute("sequences$0", i) : "stirling_error";
    px::statuspb::Code expected_status =
        i < kNumSources ? px::statuspb::Code::INTERNAL : px::statuspb::Code::OK;
    std::string expected_error = i < kNumSources ? "Initialization failed on purpose." : "";

    EXPECT_EQ(record_batch[2]->Get<StringValue>(i).string(), expected_source);
    EXPECT_EQ(record_batch[3]->Get<Int64Value>(i).val, expected_status);
    EXPECT_EQ(record_batch[4]->Get<StringValue>(i).string(), expected_error);
  }
}

}  // namespace stirling
}  // namespace px
