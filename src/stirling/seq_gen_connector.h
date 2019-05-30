#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/sequence_generator.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

/**
 * @brief Sequence Generator connector
 */
class SeqGenConnector : public SourceConnector {
 public:
  ~SeqGenConnector() override = default;

  static constexpr SourceType kSourceType = SourceType::kUnknown;

  static constexpr DataElement kElementsSeq0[] = {
      {"time_", types::DataType::TIME64NS},  {"x", types::DataType::INT64},
      {"xmod10", types::DataType::INT64},    {"xsquared", types::DataType::INT64},
      {"fibonnaci", types::DataType::INT64}, {"PIx", types::DataType::FLOAT64},
  };
  static constexpr auto kTableSeq0 = DataTableSchema("sequence_generator0", kElementsSeq0);

  static constexpr DataElement kElementsSeq1[] = {
      {"time_", types::DataType::TIME64NS},
      {"x", types::DataType::INT64},
      {"xmod8", types::DataType::INT64},
  };
  static constexpr auto kTableSeq1 = DataTableSchema("sequence_generator1", kElementsSeq1);

  static constexpr DataTableSchema kTablesArray[] = {kTableSeq0, kTableSeq1};
  static constexpr auto kTables = ConstVectorView<DataTableSchema>(kTablesArray);

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{500};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new SeqGenConnector(name));
  }

  void SetSeed(uint32_t seed) { rng_.seed(seed); }

  void ConfigureNumRowsPerGet(uint32_t min_num_rows, uint32_t max_num_rows) {
    num_rows_min_ = min_num_rows;
    num_rows_max_ = max_num_rows;
  }

  void ConfigureNumRowsPerGet(uint32_t num_rows) { ConfigureNumRowsPerGet(num_rows, num_rows); }

 protected:
  explicit SeqGenConnector(const std::string& name)
      : SourceConnector(kSourceType, name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod),
        table0_lin_seq_(1, 1),
        table0_mod10_seq_(10),
        table0_square_seq_(1, 0, 0),
        table0_pi_seq_(3.14159, 0),
        table1_lin_seq_(2, 2),
        table1_mod8_seq_(8),
        rng_(37) {}

  Status InitImpl() override { return Status::OK(); }

  void TransferDataImpl(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) override;

  Status StopImpl() override { return Status::OK(); }

 private:
  void TransferDataTable0(uint32_t num_records, uint32_t num_columns,
                          types::ColumnWrapperRecordBatch* record_batch);
  void TransferDataTable1(uint32_t num_records, uint32_t num_columns,
                          types::ColumnWrapperRecordBatch* record_batch);

  TimeSequence<int64_t> table0_time_seq_;
  LinearSequence<int64_t> table0_lin_seq_;
  ModuloSequence<int64_t> table0_mod10_seq_;
  QuadraticSequence<int64_t> table0_square_seq_;
  LinearSequence<double> table0_pi_seq_;
  FibonacciSequence<int64_t> table0_fib_seq_;

  TimeSequence<int64_t> table1_time_seq_;
  LinearSequence<int64_t> table1_lin_seq_;
  ModuloSequence<int64_t> table1_mod8_seq_;

  std::default_random_engine rng_;
  uint32_t num_rows_min_ = 0;
  uint32_t num_rows_max_ = 10;
};

}  // namespace stirling
}  // namespace pl
