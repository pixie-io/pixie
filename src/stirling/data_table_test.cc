#include <gtest/gtest.h>
#include <random>

#include "src/stirling/data_table.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/sequence_generator.h"

namespace pl {
namespace stirling {

class DataTableTest : public ::testing::Test {
 private:
  std::default_random_engine rng_;

  // The test uses a pre-defined schema.
  InfoClassSchema schema_;

  // The record size of the schema.
  size_t record_size_;

  // Test parameter: number of records to write.
  uint64_t num_records_ = 0;

  // Test parameter: max number of records per append.
  uint64_t max_append_size_;

  // Test parameter: probability of a push.
  double push_probability_ = 0.1;

  // The main Data Table (unit under test)
  std::unique_ptr<DataTable> data_table_;

 public:
  DataTableTest() : f0_seq_(1, 100), f1_seq_(3.14159, 3.14159), f2_seq_(10) {}

  /**
   * @brief Sets up the test environment, by initializing the Schema for the test.
   */
  void SetUp() override {
    // Schema for the test
    SetUpSchema();
  }

  /**
   * @brief Setup the data type.
   */
  void SetUpTable() { data_table_ = std::make_unique<DataTable>(schema_); }

  /**
   * @brief Change the random seed of the RNG.
   */
  void SetSeed(uint64_t seed) { rng_.seed(seed); }

  void InitRawData(uint64_t num_records) {
    num_records_ = num_records;
    InitRawData();
  }

  void SetPushProbability(double push_probability) { push_probability_ = push_probability; }

  void SetMaxAppendSize(uint64_t max_append_size) { max_append_size_ = max_append_size; }

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
   * Schema for our test table
   */
  void SetUpSchema() {
    schema_.push_back(InfoClassElement("f0", types::DataType::INT64));
    schema_.push_back(InfoClassElement("f1", types::DataType::FLOAT64));
    schema_.push_back(InfoClassElement("f2", types::DataType::INT64));

    record_size_ = sizeof(int64_t) + sizeof(double) + sizeof(int64_t);
  }

  /**
   * Create a row-based table in memory.
   * Row[i] = { f0(i), f1(i), f2(i) }
   * Where f0, f1 and f2 are simple functions to generate data.
   */
  void InitRawData() {
    f0_seq_.Reset();
    f1_seq_.Reset();
    f2_seq_.Reset();

    for (uint32_t i = 0; i < num_records_; ++i) {
      f0_vals_.push_back(f0_seq_());
      f1_vals_.push_back(f1_seq_());
      f2_vals_.push_back(f2_seq_());
    }
  }

  /**
   * Check that the output data matches the input functions.
   */
  void CheckColumnWrapperResult(types::ColumnWrapperRecordBatch* col_arrays, uint32_t start_record,
                                uint32_t end_record) {
    types::ColumnWrapperRecordBatch& columns = *col_arrays;

    uint32_t f_idx;
    uint32_t i;
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

    std::uniform_int_distribution<uint32_t> append_num_rows_dist(0, max_append_size_);
    std::uniform_real_distribution<double> probability_dist(0, 1.0);

    // Keep track of progress through the data
    uint32_t current_record = 0;  // Current position in source buffer
    uint32_t check_record = 0;    // Position to which the data has been compared/checked

    while (current_record < num_records_) {
      uint32_t num_rows = append_num_rows_dist(rng_);
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

      auto& columns = *(data_table_->GetActiveRecordBatch());

      for (uint32_t i = 0; i < num_rows; ++i) {
        columns[0]->Append<types::Int64Value>(f0_vals_[current_record + i]);
        columns[1]->Append<types::Float64Value>(f1_vals_[current_record + i]);
        columns[2]->Append<types::Int64Value>(f2_vals_[current_record + i]);
      }

      current_record += num_rows;

      // Periodically consume the data
      if ((probability_dist(rng_) < push_probability_) || last_pass) {
        auto data_batches_ptr = data_table_->GetRecordBatches();
        auto data_batches_ptr_raw = data_batches_ptr.ValueOrDie().get();
        for (const auto& data_batch : *data_batches_ptr_raw) {
          CheckColumnWrapperResult(data_batch.get(), check_record, current_record);
        }
        check_record = current_record;
      }
    }
  }
};

constexpr uint32_t kNumRecords = 1 << 16;
constexpr uint64_t kRNGSeed = 37;
constexpr std::array<double, 3> kPushProbability = {0.01, 0.1, 0.5};
constexpr std::array<uint64_t, 3> kMaxAppendSize = {20, 200, 2000};

/**
 * Test Data Tables.
 *
 * Test continuously writes (Appends) data from a "source" into the tables, with different batch
 * sizes. With some probability--between the Appends--the data is also flushed out into a "sink".
 *
 * On every flush, the data is checked to see if it matches the known data pattern that was being
 * generated.
 */
TEST_F(DataTableTest, column_wrapper_read_write) {
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
}  // namespace pl
