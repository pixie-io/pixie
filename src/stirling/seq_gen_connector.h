#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "src/common/common.h"
#include "src/stirling/sequence_generator.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

/**
 * @brief Sequence Generator connector
 */
class SeqGenConnector : public SourceConnector {
 public:
  virtual ~SeqGenConnector() = default;

  static constexpr SourceType kSourceType = SourceType::kUnknown;

  static constexpr char kName[] = "sequence_generator";

  inline static const std::chrono::milliseconds kDefaultSamplingPeriod{500};
  inline static const std::chrono::milliseconds kDefaultPushPeriod{1000};

  inline static const DataElements kElements = {
      DataElement("time_", DataType::TIME64NS), DataElement("x", DataType::INT64),
      DataElement("x%10", DataType::INT64),     DataElement("x^2", DataType::INT64),
      DataElement("Fib(x)", DataType::INT64),   DataElement("PI*x", DataType::FLOAT64)};

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
      : SourceConnector(kSourceType, name, kElements),
        time_seq_(),
        lin_seq_(1, 1),
        mod10_seq_(10),
        square_seq_(1, 0, 0),
        pi_seq_(3.14159, 0),
        fib_seq_(),
        rng_(37) {}

  Status InitImpl() override { return Status::OK(); }

  RawDataBuf GetDataImpl() override;

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

  std::vector<uint8_t> data_buf_;
};

}  // namespace stirling
}  // namespace pl
