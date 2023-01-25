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

#include <ctime>
#include <functional>
#include <iomanip>
#include <thread>
#include <unordered_map>
#include <utility>

#include <absl/functional/bind_front.h>
#include <absl/strings/str_format.h>

#include "src/common/base/base.h"
#include "src/common/base/hash_utils.h"
#include "src/common/testing/testing.h"
#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"
#include "src/stirling/source_connectors/seq_gen/sequence_generator.h"
#include "src/stirling/stirling.h"

#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/dynamic_tracer.h"

#include "src/stirling/proto/stirling.pb.h"

using px::stirling::DataElement;
using px::stirling::DataTableSchema;
using px::stirling::SeqGenConnector;
using px::stirling::SourceRegistry;
using px::stirling::Stirling;

using px::types::ColumnWrapperRecordBatch;
using px::types::DataType;
using px::types::Float64Value;
using px::types::Int64Value;
using px::types::StringValue;
using px::types::TabletID;
using px::types::Time64NSValue;

using px::ArrayView;
using px::Status;

// Test arguments, from the command line
DEFINE_uint64(kRNGSeed, 377, "Random Seed");
DEFINE_uint32(kNumSources, 2, "Number of sources");
DEFINE_uint32(kNumIterMin, 10, "Min number of iterations");
DEFINE_uint32(kNumIterMax, 20, "Max number of iterations");
DEFINE_uint64(kNumProcessedRequirement, 5000,
              "Number of records required to be processed before test is allowed to end");

namespace px {
namespace stirling {

// This is the duration for which a subscription will be valid.
constexpr std::chrono::milliseconds kDurationPerIter{500};

// If test typically takes too long, you may want to reduce kNumIterMin or kNumProcessedRequirement.
// Note that kNumIterMax * kDurationPerIter defines the maximum time the test can take.

// This struct is used as a unordered_map key, to uniquely identify a sequence.
// Each column in each tablet of each table is mapped to a unique, deterministic sequence of values.
struct TableTabletCol {
  uint64_t table;
  TabletID tablet;
  size_t col;

  bool operator==(const TableTabletCol& other) const {
    return (table == other.table) && (tablet == other.tablet) && (col == other.col);
  }
};

// Hash function required to use TableTabletCol as an unordered_map key.
class TableTabletColHashFn {
 public:
  size_t operator()(const TableTabletCol& val) const {
    size_t hash = 0;
    hash = px::HashCombine(hash, val.table);
    hash = px::HashCombine(hash, std::hash<TabletID>{}(val.tablet));
    hash = px::HashCombine(hash, val.col);
    return hash;
  }
};

/**
 * StirlingTest is an end-to-end test of Stirling. It uses the Sequence Generator connector
 * as a deterministic data source. It then periodically samples this data source,
 * and also periodically pushes the data through the registered callback.
 *
 * The registered callback ensures that the sequence of data values pushed matches
 * the expected sequence. If any value in the sequence is missing, or no data is pushed
 * at all, then it means that something is busted.
 */
class StirlingTest : public ::testing::Test {
 protected:
  std::unique_ptr<Stirling> stirling_;
  stirlingpb::Publish publish_proto_;

  // Schemas
  std::unordered_map<uint32_t, DataTableSchema> schemas_;

  // Reference model (checkers).
  std::unordered_map<TableTabletCol, std::unique_ptr<px::stirling::Sequence<int64_t>>,
                     TableTabletColHashFn>
      int_seq_checker_;
  std::unordered_map<TableTabletCol, std::unique_ptr<px::stirling::Sequence<double>>,
                     TableTabletColHashFn>
      double_seq_checker_;

  std::unordered_map<uint32_t, uint64_t> num_processed_per_table_;
  std::atomic<uint64_t> num_processed_;

  // Random distributions for test parameters.
  std::default_random_engine rng;
  std::uniform_int_distribution<uint32_t> push_period_millis_dist_;
  std::uniform_real_distribution<double> uniform_probability_dist_;

  inline static const uint64_t& kRNGSeed = FLAGS_kRNGSeed;
  inline static const uint32_t& kNumSources = FLAGS_kNumSources;
  inline static const uint32_t& kNumIterMin = FLAGS_kNumIterMin;
  inline static const uint32_t& kNumIterMax = FLAGS_kNumIterMax;
  inline static const uint64_t& kNumProcessedRequirement = FLAGS_kNumProcessedRequirement;

  StirlingTest()
      : rng(kRNGSeed), push_period_millis_dist_(0, 100), uniform_probability_dist_(0, 1.0) {}

  void SetUp() override {
    // Make registry with a number of SeqGenConnectors.
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    for (uint32_t i = 0; i < kNumSources; ++i) {
      registry->RegisterOrDie<SeqGenConnector>(absl::Substitute("sequences$0", i));
    }

    stirling_ = Stirling::Create(std::move(registry));

    // Set a callback function that receives the data.
    stirling_->RegisterDataPushCallback(absl::bind_front(&StirlingTest::AppendData, this));

    stirling_->GetPublishProto(&publish_proto_);

    // Create reference model
    for (const auto& info_class : publish_proto_.published_info_classes()) {
      const uint64_t id = info_class.id();
      const std::string& name = info_class.schema().name();
      if (name[name.length() - 1] == '0') {
        // Table 0 is a simple (non-tabletized) table with multiple columns.
        // Here we append checkers that mimics the sequences from kSeq0Table from
        // seq_gen_connector.h.

        schemas_.emplace(id, SeqGenConnector::kSeq0Table);

        uint32_t col_idx = 1;  // Start at 1, because column 0 is time.
        int_seq_checker_.emplace(TableTabletCol{id, "", col_idx++},
                                 std::make_unique<px::stirling::LinearSequence<int64_t>>(1, 1));
        int_seq_checker_.emplace(TableTabletCol{id, "", col_idx++},
                                 std::make_unique<px::stirling::ModuloSequence<int64_t>>(10));
        int_seq_checker_.emplace(
            TableTabletCol{id, "", col_idx++},
            std::make_unique<px::stirling::QuadraticSequence<int64_t>>(1, 0, 0));
        int_seq_checker_.emplace(TableTabletCol{id, "", col_idx++},
                                 std::make_unique<px::stirling::FibonacciSequence<int64_t>>());
        double_seq_checker_.emplace(
            TableTabletCol{id, "", col_idx++},
            std::make_unique<px::stirling::LinearSequence<double>>(3.14159, 0));

        num_processed_per_table_.emplace(id, 0);
      } else if (name[name.length() - 1] == '1') {
        // Table 0 is a tabletized table.
        // Here we append checkers that mimics the sequences from kSeq1Table from
        // seq_gen_connector.h. The modulo column from the table is used to form 8 distinct tablets.

        schemas_.emplace(id, SeqGenConnector::kSeq1Table);

        // Spread seq_gen's second table into tablets, based on its modulo column (mod 8).
        static const uint32_t kModulo = 8;
        for (uint32_t t = 0; t < kModulo; ++t) {
          std::string t_str = std::to_string(t);
          uint32_t col_idx = 1;  // Start at 1, because column 0 is time.
          int_seq_checker_.emplace(
              TableTabletCol{id, t_str, col_idx++},
              std::make_unique<px::stirling::LinearSequence<int64_t>>(2 * kModulo, 2 + (2 * t)));

          // The modulo is the tablet id, so after tabletization, it will appear as a constant.
          // So use a Linear Sequence with slope 0 to specify a constant expectation.
          int_seq_checker_.emplace(TableTabletCol{id, t_str, col_idx++},
                                   std::make_unique<px::stirling::LinearSequence<int64_t>>(0, t));
        }

        num_processed_per_table_.emplace(id, 0);
      }
    }

    num_processed_ = 0;
  }

  void TearDown() override {
    for (const auto& info_class : publish_proto_.published_info_classes()) {
      LOG(INFO) << absl::Substitute("Number of records processed: $0",
                                    num_processed_per_table_[info_class.id()]);
    }
  }

  Status AppendData(uint64_t table_id, TabletID tablet_id,
                    std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
    // Note: Implicit assumption (not checked here) is that all columns have the same size
    size_t num_records = (*record_batch)[0]->Size();
    CheckRecordBatch(table_id, tablet_id, num_records, *record_batch);
    return Status::OK();
  }

  void CheckRecordBatch(const uint64_t table_id, TabletID tablet_id, size_t num_records,
                        const ColumnWrapperRecordBatch& record_batch) {
    auto schema_elements = schemas_.at(table_id).elements();

    for (size_t i = 0; i < num_records; ++i) {
      uint32_t j = 0;

      for (const auto& col : record_batch) {
        TableTabletCol key{table_id, tablet_id, j};

        switch (schema_elements[j].type()) {
          case DataType::TIME64NS: {
            auto val = col->Get<Time64NSValue>(i).val;
            PX_UNUSED(val);
          } break;
          case DataType::INT64: {
            auto& checker = *(int_seq_checker_.at(key));
            EXPECT_EQ(checker(), col->Get<Int64Value>(i).val);
          } break;
          case DataType::FLOAT64: {
            auto& checker = *(double_seq_checker_.at(key));
            EXPECT_EQ(checker(), col->Get<Float64Value>(i).val);
          } break;
          default:
            CHECK(false) << absl::Substitute("Unrecognized type: $0",
                                             ToString(schema_elements[j].type()));
        }

        j++;
      }
    }

    num_processed_per_table_[table_id] += num_records;
    num_processed_ += num_records;
  }

  uint64_t NumProcessed() { return num_processed_; }
};

// Stress/regression test that hammers Stirling with sequences.
// A reference model checks the sequences are correct on the callback.
// This version uses synchronized subscriptions that occur while Stirling is stopped.
TEST_F(StirlingTest, hammer_time_on_stirling_synchronized_subscriptions) {
  uint32_t i = 0;
  while (NumProcessed() < kNumProcessedRequirement || i < kNumIterMin) {
    // Run Stirling data collector.
    ASSERT_OK(stirling_->RunAsThread());

    // Stay in this config for the specified amount of time.
    std::this_thread::sleep_for(kDurationPerIter);

    stirling_->Stop();

    i++;

    // In case we have a slow environment, break out of the test after some time.
    if (i > kNumIterMax) {
      break;
    }
  }

  EXPECT_GT(NumProcessed(), 0);
}

// Stress/regression test that hammers Stirling with sequences.
// A reference model checks the sequences are correct on the callback.
// This version uses on-the-fly subscriptions that occur while Stirling is running.
TEST_F(StirlingTest, hammer_time_on_stirling_on_the_fly_subs) {
  // Run Stirling data collector.
  ASSERT_OK(stirling_->RunAsThread());

  std::this_thread::sleep_for(kDurationPerIter);

  uint32_t i = 0;
  while (NumProcessed() < kNumProcessedRequirement || i < kNumIterMin) {
    // Stay in this config for the specified amount of time..
    std::this_thread::sleep_for(kDurationPerIter);

    i++;

    // In case we have a slow environment, break out of the test after some time.
    if (i > kNumIterMax) {
      break;
    }
  }

  stirling_->Stop();

  EXPECT_GT(NumProcessed(), 0);
}

TEST_F(StirlingTest, no_data_callback_defined) {
  stirling_->RegisterDataPushCallback(nullptr);

  // Should fail to run as a Stirling-managed thread.
  EXPECT_NOT_OK(stirling_->RunAsThread());

  // Should also fail to run as a caller-managed thread,
  // which means it should be immediately joinable.
  std::thread run_thread = std::thread(&Stirling::Run, stirling_.get());
  ASSERT_TRUE(run_thread.joinable());
  run_thread.join();
}

}  // namespace stirling
}  // namespace px
