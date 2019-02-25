#include <gtest/gtest.h>
#include <random>

#include "src/stirling/data_table.h"
#include "src/stirling/info_class_schema.h"

namespace pl {
namespace stirling {

using types::DataType;

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

  // The source buffer of data
  std::vector<uint8_t> buffer_vector_;

  // The main Data Table (unit under test)
  std::unique_ptr<DataTable> data_table_;

 public:
  DataTableTest() {}

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
  void SetUpTable(TableType type) {
    switch (type) {
      case TableType::ColumnWrapper:
        data_table_ = std::make_unique<ColumnWrapperDataTable>(schema_);
        break;
      case TableType::Arrow:
        data_table_ = std::make_unique<ArrowDataTable>(schema_);
        break;
      default:
        CHECK(0) << "Unknown data type";
    }
  }

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
  // TODO(oazizi): Use Sequences in seqgen_connector.h (and perhaps refactor).
  int64_t f0(uint32_t x) { return x + 100; }

  double f1(uint32_t x) { return 3.14 * (x + 1); }

  int64_t f2(uint32_t x) { return x % 10; }

  /**
   * Schema for our test table
   */
  void SetUpSchema() {
    schema_.push_back(
        InfoClassElement("f0", DataType::INT64, Element_State::Element_State_SUBSCRIBED));
    schema_.push_back(
        InfoClassElement("f1", DataType::FLOAT64, Element_State::Element_State_SUBSCRIBED));
    schema_.push_back(
        InfoClassElement("f2", DataType::INT64, Element_State::Element_State_SUBSCRIBED));

    record_size_ = sizeof(int64_t) + sizeof(double) + sizeof(int64_t);
  }

  /**
   * Create a row-based table in memory.
   * Row[i] = { f0(i), f1(i), f2(i) }
   * Where f0, f1 and f2 are simple functions to generate data.
   */
  void InitRawData() {
    buffer_vector_.resize(num_records_ * record_size_);
    uint8_t* buf = buffer_vector_.data();

    size_t offset = 0;
    for (uint32_t i = 0; i < num_records_; ++i) {
      reinterpret_cast<int64_t*>(buf)[offset / sizeof(int64_t)] = f0(i);
      offset += sizeof(int64_t);

      reinterpret_cast<double*>(buf)[offset / sizeof(double)] = f1(i);
      offset += sizeof(double);

      reinterpret_cast<int64_t*>(buf)[offset / sizeof(int64_t)] = f2(i);
      offset += sizeof(int64_t);
    }
  }

  /**
   * Check that the output data matches the input functions.
   */
  void CheckColumnWrapperResult(ColumnWrapperRecordBatch* col_arrays, uint32_t start_record,
                                uint32_t end_record) {
    uint32_t f_idx;
    uint32_t i;
    for (f_idx = start_record, i = 0; f_idx < end_record; ++f_idx, ++i) {
      auto col0 = std::static_pointer_cast<carnot::udf::Int64ValueColumnWrapper>((*col_arrays)[0]);
      auto col1 =
          std::static_pointer_cast<carnot::udf::Float64ValueColumnWrapper>((*col_arrays)[1]);
      auto col2 = std::static_pointer_cast<carnot::udf::Int64ValueColumnWrapper>((*col_arrays)[2]);

      auto col0_val = (*col0)[i].val;
      auto col1_val = (*col1)[i].val;
      auto col2_val = (*col2)[i].val;

      EXPECT_EQ(col0_val, f0(f_idx));
      EXPECT_DOUBLE_EQ(col1_val, f1(f_idx));
      EXPECT_EQ(col2_val, f2(f_idx));
    }
  }

  /**
   * Check that the output data matches the input functions.
   */
  void CheckArrowResult(std::shared_ptr<arrow::RecordBatch> record_batch, uint32_t start_record,
                        uint32_t end_record) {
    uint32_t f_idx;
    uint32_t i;
    for (f_idx = start_record, i = 0; f_idx < end_record; ++f_idx, ++i) {
      auto col0 = std::static_pointer_cast<arrow::Int64Array>(record_batch->column(0));
      auto col1 = std::static_pointer_cast<arrow::DoubleArray>(record_batch->column(1));
      auto col2 = std::static_pointer_cast<arrow::Int64Array>(record_batch->column(2));

      auto col0_val = col0->Value(i);
      auto col1_val = col1->Value(i);
      auto col2_val = col2->Value(i);

      EXPECT_EQ(col0_val, f0(f_idx));
      EXPECT_DOUBLE_EQ(col1_val, f1(f_idx));
      EXPECT_EQ(col2_val, f2(f_idx));
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
    std::uniform_int_distribution<uint32_t> append_num_rows_dist(0, max_append_size_);
    std::uniform_real_distribution<double> probability_dist(0, 1.0);

    uint8_t* source_buffer = buffer_vector_.data();

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

      // Get a pointer into the right part of the source buffer
      uint8_t* current_buf = source_buffer + current_record * record_size_;

      // Send the data to the Data Table
      Status s = data_table_->AppendData(current_buf, num_rows);
      current_record += num_rows;

      EXPECT_OK(s);

      // Periodically consume the data
      if ((probability_dist(rng_) < push_probability_) || last_pass) {
        switch (data_table_->table_type()) {
          case TableType::ColumnWrapper: {
            auto data_batches_ptr = data_table_->GetColumnWrapperRecordBatches();
            auto data_batches_ptr_raw = data_batches_ptr.ValueOrDie().get();
            for (const auto& data_batch : *data_batches_ptr_raw) {
              CheckColumnWrapperResult(data_batch.get(), check_record, current_record);
            }
          } break;
          case TableType::Arrow: {
            auto data_batches_ptr = data_table_->GetArrowRecordBatches();
            auto data_batches_ptr_raw = data_batches_ptr.ValueOrDie().get();
            for (const auto& data_batch_i : *data_batches_ptr_raw) {
              CheckArrowResult(data_batch_i, check_record, current_record);
            }
          } break;
          default:
            CHECK(false) << "No test for this type of Data table";
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
      SetUpTable(TableType::ColumnWrapper);

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

/**
 * Same as above, but with Arrow Tables
 */
TEST_F(DataTableTest, arrow_read_write) {
  SetSeed(kRNGSeed);

  for (auto push_probability : kPushProbability) {
    for (auto max_append_size : kMaxAppendSize) {
      SetUpTable(TableType::Arrow);

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
