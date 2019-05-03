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

  static constexpr char kName[] = "sequence_generator";

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{500};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  inline static const DataElements kElements = {DataElement("time_", types::DataType::TIME64NS),
                                                DataElement("x", types::DataType::INT64),
                                                DataElement("xmod10", types::DataType::INT64),
                                                DataElement("xsquared", types::DataType::INT64),
                                                DataElement("fibonnaci", types::DataType::INT64),
                                                DataElement("PIx", types::DataType::FLOAT64)};

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
      : SourceConnector(kSourceType, name, kElements, kDefaultSamplingPeriod, kDefaultPushPeriod),
        lin_seq_(1, 1),
        mod10_seq_(10),
        square_seq_(1, 0, 0),
        pi_seq_(3.14159, 0),
        rng_(37) {}

  Status InitImpl() override { return Status::OK(); }

  void TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) override;

  Status StopImpl() override { return Status::OK(); }

 private:
  TimeSequence<int64_t> time_seq_;
  LinearSequence<int64_t> lin_seq_;
  ModuloSequence<int64_t> mod10_seq_;
  QuadraticSequence<int64_t> square_seq_;
  LinearSequence<double> pi_seq_;
  FibonacciSequence<int64_t> fib_seq_;

  std::default_random_engine rng_;
  uint32_t num_rows_min_ = 0;
  uint32_t num_rows_max_ = 10;
};

}  // namespace stirling
}  // namespace pl
