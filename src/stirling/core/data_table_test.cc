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

#include <gtest/gtest.h>
#include <random>
#include <string>

#include "src/stirling/core/data_table.h"
#include "src/stirling/source_connectors/seq_gen/sequence_generator.h"

namespace px {
namespace stirling {

class DataTableTest : public ::testing::Test {
 protected:
  // The test uses a pre-defined schema.
  static constexpr DataElement kElements[] = {
      {"time_", "time", types::DataType::TIME64NS, types::SemanticType::ST_NONE,
       types::PatternType::METRIC_COUNTER},
      {"x", "an int value", types::DataType::INT64, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"s", "a string", types::DataType::STRING, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
  };
  static constexpr auto kSchema =
      DataTableSchema("test_table", "This is the table description", kElements);

  DataTableTest() : data_table_(std::make_unique<DataTable>(/*id*/ 0, kSchema)) {}

  std::unique_ptr<DataTable> data_table_;
};

TEST_F(DataTableTest, ResultIsSorted) {
  std::vector<int> time_vals = {0, 10, 40, 20, 30, 50, 90, 70, 60, 80};
  std::vector<int> x_vals = {0, 1, 4, 2, 3, 5, 9, 7, 6, 8};
  std::vector<std::string> s_vals = {"a", "b", "e", "c", "d", "f", "j", "h", "g", "i"};

  for (size_t i = 0; i < time_vals.size(); ++i) {
    DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
    r.Append<r.ColIndex("time_")>(time_vals[i]);
    r.Append<r.ColIndex("x")>(x_vals[i]);
    r.Append<r.ColIndex("s")>(s_vals[i]);
  }

  std::vector<TaggedRecordBatch> record_batches = data_table_->ConsumeRecords();

  ASSERT_EQ(record_batches.size(), 1);
  types::ColumnWrapperRecordBatch& rb = record_batches[0].records;

  for (size_t i = 0; i < time_vals.size(); ++i) {
    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(i), 10 * static_cast<int>(i));
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(i), static_cast<int>(i));
    EXPECT_EQ(rb[2]->Get<types::StringValue>(i), std::string(1, 'a' + i));
  }
}

// No time passed to RecordBuilder, so all timestamps should be zero.
// That means there should never be any expired or carry-over records.
// Also, nothing should be sorted in any way.
TEST_F(DataTableTest, FixedTimeMode) {
  std::vector<int> time_vals = {0, 10, 40, 20, 30, 50, 90, 70, 60, 80};
  std::vector<int> x_vals = {0, 1, 4, 2, 3, 5, 9, 7, 6, 8};
  std::vector<std::string> s_vals = {"a", "b", "e", "c", "d", "f", "j", "h", "g", "i"};

  for (size_t i = 0; i < time_vals.size(); ++i) {
    DataTable::RecordBuilder<&kSchema> r(data_table_.get());
    r.Append<r.ColIndex("time_")>(time_vals[i]);
    r.Append<r.ColIndex("x")>(x_vals[i]);
    r.Append<r.ColIndex("s")>(s_vals[i]);

    std::vector<TaggedRecordBatch> record_batches = data_table_->ConsumeRecords();

    ASSERT_EQ(record_batches.size(), 1);
    types::ColumnWrapperRecordBatch& rb = record_batches[0].records;

    ASSERT_EQ(rb[0]->Size(), 1);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), time_vals[i]);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), x_vals[i]);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), s_vals[i]);
  }
}

TEST_F(DataTableTest, FixedTimeModeV2) {
  std::vector<int> time_vals = {0, 10, 40, 20, 30, 50, 90, 70, 60, 80};
  std::vector<int> x_vals = {0, 1, 4, 2, 3, 5, 9, 7, 6, 8};
  std::vector<std::string> s_vals = {"a", "b", "e", "c", "d", "f", "j", "h", "g", "i"};

  for (size_t i = 0; i < time_vals.size(); ++i) {
    DataTable::RecordBuilder<&kSchema> r(data_table_.get());
    r.Append<r.ColIndex("time_")>(time_vals[i]);
    r.Append<r.ColIndex("x")>(x_vals[i]);
    r.Append<r.ColIndex("s")>(s_vals[i]);
  }

  std::vector<TaggedRecordBatch> record_batches = data_table_->ConsumeRecords();

  ASSERT_EQ(record_batches.size(), 1);
  types::ColumnWrapperRecordBatch& rb = record_batches[0].records;

  ASSERT_EQ(rb[0]->Size(), 10);

  for (size_t i = 0; i < time_vals.size(); ++i) {
    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(i), time_vals[i]);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(i), x_vals[i]);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(i), s_vals[i]);
  }
}

TEST_F(DataTableTest, Expiry) {
  std::vector<int> time_vals = {0, 10, 40, 20, 30, 50, 90, 70, 60, 80};
  std::vector<int> x_vals = {0, 1, 4, 2, 3, 5, 9, 7, 6, 8};
  std::vector<std::string> s_vals = {"a", "b", "e", "c", "d", "f", "j", "h", "g", "i"};

  // Process the first four entries. None should be expired.
  {
    for (size_t i = 0; i < 4; ++i) {
      DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
      r.Append<r.ColIndex("time_")>(time_vals[i]);
      r.Append<r.ColIndex("x")>(x_vals[i]);
      r.Append<r.ColIndex("s")>(s_vals[i]);
    }

    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 1);
    types::ColumnWrapperRecordBatch& rb = tablets[0].records;

    ASSERT_EQ(rb[0]->Size(), 4);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), 0);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), 0);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), "a");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(1), 10);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(1), 1);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(1), "b");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(2), 20);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(2), 2);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(2), "c");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(3), 40);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(3), 4);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(3), "e");
  }

  // Process the next three entries. Time 30 should be expired.
  {
    for (size_t i = 4; i < 7; ++i) {
      DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
      r.Append<r.ColIndex("time_")>(time_vals[i]);
      r.Append<r.ColIndex("x")>(x_vals[i]);
      r.Append<r.ColIndex("s")>(s_vals[i]);
    }

    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 1);
    types::ColumnWrapperRecordBatch& rb = tablets[0].records;

    // Only expect 2 out of 3 records, because one should have been expired.
    ASSERT_EQ(rb[0]->Size(), 2);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), 50);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), 5);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), "f");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(1), 90);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(1), 9);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(1), "j");
  }

  // Process the next three entries. Time 30 should be expired.
  {
    for (size_t i = 7; i < 10; ++i) {
      DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
      r.Append<r.ColIndex("time_")>(time_vals[i]);
      r.Append<r.ColIndex("x")>(x_vals[i]);
      r.Append<r.ColIndex("s")>(s_vals[i]);
    }

    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 0);
  }
}

// This test has scrambled entries, but ConsumeRecords is called with end times
// that should cause carryover. This test also causes no expirations for simplicity.
TEST_F(DataTableTest, Carryover) {
  std::vector<int> time_vals = {0, 10, 40, 20, 30, 50, 90, 70, 60, 80};
  std::vector<int> x_vals = {0, 1, 4, 2, 3, 5, 9, 7, 6, 8};
  std::vector<std::string> s_vals = {"a", "b", "e", "c", "d", "f", "j", "h", "g", "i"};

  // Process the first four entries. None should be expired.
  {
    for (size_t i = 0; i < 4; ++i) {
      DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
      r.Append<r.ColIndex("time_")>(time_vals[i]);
      r.Append<r.ColIndex("x")>(x_vals[i]);
      r.Append<r.ColIndex("s")>(s_vals[i]);
    }

    data_table_->SetConsumeRecordsCutoffTime(20);
    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 1);
    types::ColumnWrapperRecordBatch& rb = tablets[0].records;

    ASSERT_EQ(rb[0]->Size(), 3);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), 0);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), 0);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), "a");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(1), 10);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(1), 1);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(1), "b");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(2), 20);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(2), 2);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(2), "c");
  }

  // Process the next three entries. Time 30 should be expired.
  {
    for (size_t i = 4; i < 7; ++i) {
      DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
      r.Append<r.ColIndex("time_")>(time_vals[i]);
      r.Append<r.ColIndex("x")>(x_vals[i]);
      r.Append<r.ColIndex("s")>(s_vals[i]);
    }

    data_table_->SetConsumeRecordsCutoffTime(40);
    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 1);
    types::ColumnWrapperRecordBatch& rb = tablets[0].records;

    // Only expect 2 out of 3 records, because one should have been expired.
    ASSERT_EQ(rb[0]->Size(), 2);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), 30);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), 3);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), "d");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(1), 40);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(1), 4);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(1), "e");
  }

  // Process the next three entries. Time 30 should be expired.
  {
    for (size_t i = 7; i < 10; ++i) {
      DataTable::RecordBuilder<&kSchema> r(data_table_.get(), time_vals[i]);
      r.Append<r.ColIndex("time_")>(time_vals[i]);
      r.Append<r.ColIndex("x")>(x_vals[i]);
      r.Append<r.ColIndex("s")>(s_vals[i]);
    }

    data_table_->SetConsumeRecordsCutoffTime(70);
    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 1);
    types::ColumnWrapperRecordBatch& rb = tablets[0].records;

    ASSERT_EQ(rb[0]->Size(), 3);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), 50);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), 5);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), "f");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(1), 60);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(1), 6);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(1), "g");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(2), 70);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(2), 7);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(2), "h");
  }

  {
    data_table_->SetConsumeRecordsCutoffTime(100);
    std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();

    ASSERT_EQ(tablets.size(), 1);
    types::ColumnWrapperRecordBatch& rb = tablets[0].records;

    ASSERT_EQ(rb[0]->Size(), 2);

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(0), 80);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(0), 8);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(0), "i");

    EXPECT_EQ(rb[0]->Get<types::Time64NSValue>(1), 90);
    EXPECT_EQ(rb[1]->Get<types::Int64Value>(1), 9);
    EXPECT_EQ(rb[2]->Get<types::StringValue>(1), "j");
  }
}

class DataTableStressTest : public ::testing::Test {
 private:
  std::default_random_engine rng_;

  // The test uses a pre-defined schema.
  static constexpr DataElement kElements[] = {
      {"f0", "f(x) = x+100", types::DataType::INT64, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"f1", "f(x) = 3.14159*x+3.14159", types::DataType::FLOAT64, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"f2", "f(x) = x % 10", types::DataType::INT64, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL_ENUM},
  };
  static constexpr auto kSchema = DataTableSchema("test_table", "A table of sequences", kElements);

  // Test parameter: number of records to write.
  size_t num_records_ = 0;

  // Test parameter: max number of records per append.
  size_t max_append_size_ = 10;

  // Test parameter: probability of a push.
  double push_probability_ = 0.1;

  // The main Data Table (unit under test)
  std::unique_ptr<DataTable> data_table_;

 public:
  DataTableStressTest() : f0_seq_(1, 100), f1_seq_(3.14159, 3.14159), f2_seq_(10) {}

  /**
   * @brief Sets up the test environment, by initializing the Schema for the test.
   */
  void SetUp() override {}

  /**
   * @brief Setup the data type.
   */
  void SetUpTable() { data_table_ = std::make_unique<DataTable>(/*id*/ 0, kSchema); }

  /**
   * @brief Change the random seed of the RNG.
   */
  void SetSeed(uint64_t seed) { rng_.seed(seed); }

  void InitRawData(size_t num_records) {
    num_records_ = num_records;
    InitRawData();
  }

  void SetPushProbability(double push_probability) { push_probability_ = push_probability; }

  void SetMaxAppendSize(size_t max_append_size) { max_append_size_ = max_append_size; }

  void RunAndCheck() { RunAndCheckImpl(); }

 private:
  /**
   *  A set of pre-defined sequences. Useful, because they are easy to check.
   */
  LinearSequence<int64_t> f0_seq_;
  LinearSequence<double> f1_seq_;
  ModuloSequence<int64_t> f2_seq_;

  std::vector<int64_t> f0_vals_;
  std::vector<double> f1_vals_;
  std::vector<int64_t> f2_vals_;

  /**
   * Create a row-based table in memory.
   * Row[i] = { f0(i), f1(i), f2(i) }
   * Where f0, f1 and f2 are simple functions to generate data.
   */
  void InitRawData() {
    f0_seq_.Reset();
    f1_seq_.Reset();
    f2_seq_.Reset();

    for (size_t i = 0; i < num_records_; ++i) {
      f0_vals_.push_back(f0_seq_());
      f1_vals_.push_back(f1_seq_());
      f2_vals_.push_back(f2_seq_());
    }
  }

  /**
   * Check that the output data matches the input functions.
   */
  void CheckColumnWrapperResult(const types::ColumnWrapperRecordBatch& columns, size_t start_record,
                                size_t end_record) {
    size_t f_idx;
    size_t i;
    for (f_idx = start_record, i = 0; f_idx < end_record; ++f_idx, ++i) {
      auto col0_val = columns[0]->Get<types::Int64Value>(i).val;
      auto col1_val = columns[1]->Get<types::Float64Value>(i).val;
      auto col2_val = columns[2]->Get<types::Int64Value>(i).val;

      EXPECT_EQ(col0_val, f0_seq_());
      EXPECT_DOUBLE_EQ(col1_val, f1_seq_());
      EXPECT_EQ(col2_val, f2_seq_());
    }
  }

  /**
   * Main test function.
   *
   * Test continuously writes (Appends) data from a "source" into the tables, with different batch
   * sizes. With some probability--between the Appends--the data is also flushed out into a "sink".
   *
   * On every flush, the data is checked to see if it matches the known data pattern that was being
   * generated.
   */
  void RunAndCheckImpl() {
    f0_seq_.Reset();
    f1_seq_.Reset();
    f2_seq_.Reset();

    std::uniform_int_distribution<size_t> append_num_rows_dist(0, max_append_size_);
    std::uniform_real_distribution<double> probability_dist(0, 1.0);

    // Keep track of progress through the data
    size_t current_record = 0;  // Current position in source buffer
    size_t check_record = 0;    // Position to which the data has been compared/checked

    while (current_record < num_records_) {
      size_t num_rows = append_num_rows_dist(rng_);
      bool last_pass = false;

      // If we would go out of bounds of the source buffer
      // Need to make some adjustments
      if (current_record + num_rows >= num_records_) {
        // Adjust number of rows down to the last remaining rows
        num_rows = num_records_ - current_record;

        // Check for a test issue
        CHECK_NE(0ULL, num_rows);

        last_pass = true;
      }

      for (size_t i = 0; i < num_rows; ++i) {
        DataTable::RecordBuilder<&kSchema> r(data_table_.get());
        r.Append<r.ColIndex("f0")>(f0_vals_[current_record + i]);
        r.Append<r.ColIndex("f1")>(f1_vals_[current_record + i]);
        r.Append<r.ColIndex("f2")>(f2_vals_[current_record + i]);
      }

      current_record += num_rows;

      // Periodically consume the data
      if ((probability_dist(rng_) < push_probability_) || last_pass) {
        auto data_batches = data_table_->ConsumeRecords();
        for (const auto& data_batch : data_batches) {
          CheckColumnWrapperResult(data_batch.records, check_record, current_record);
        }
        check_record = current_record;
      }
    }
  }
};

constexpr size_t kNumRecords = 1 << 16;
constexpr uint64_t kRNGSeed = 37;
constexpr std::array<double, 3> kPushProbability = {0.01, 0.1, 0.5};
constexpr std::array<size_t, 3> kMaxAppendSize = {20, 200, 2000};

/**
 * Test Data Tables.
 *
 * Test continuously writes (Appends) data from a "source" into the tables, with different batch
 * sizes. With some probability--between the Appends--the data is also flushed out into a "sink".
 *
 * On every flush, the data is checked to see if it matches the known data pattern that was being
 * generated.
 */
TEST_F(DataTableStressTest, column_wrapper_read_write) {
  SetSeed(kRNGSeed);

  for (auto push_probability : kPushProbability) {
    for (auto max_append_size : kMaxAppendSize) {
      SetUpTable();

      SetPushProbability(push_probability);

      SetMaxAppendSize(max_append_size);

      InitRawData(kNumRecords);

      // Test appends the data into the data table in batches.
      // Between certain batches, the data is consumed from the data table,
      // and inspected for consistency with expected data pattern.
      RunAndCheck();
    }
  }
}

}  // namespace stirling
}  // namespace px
