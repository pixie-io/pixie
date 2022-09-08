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

#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/core/source_connector.h"
#include "src/stirling/source_connectors/seq_gen/sequence_generator.h"

namespace px {
namespace stirling {

/**
 * @brief Sequence Generator connector
 */
class SeqGenConnector : public SourceConnector {
 public:
  static constexpr std::string_view kName = "sequences";
  static constexpr auto kSamplingPeriod = std::chrono::milliseconds{500};
  static constexpr auto kPushPeriod = std::chrono::milliseconds{1000};

  // clang-format off
  static constexpr DataElement kElementsSeq0[] = {
      {"time_",
       "Timestamp when the data record was collected.",
       types::DataType::TIME64NS,
       types::SemanticType::ST_NONE,
       types::PatternType::METRIC_COUNTER},
      {"x",
       "A sequence number.",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"xmod10",
       "The value of x % 10.",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL_ENUM},
      {"xsquared",
       "The value of x^2.",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"fibonnaci", "Fibonnaci number",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"PIx", "PI * x",
       types::DataType::FLOAT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
  };
  // clang-format on
  static constexpr auto kSeq0Table =
      DataTableSchema("sequence_generator0",
                      "A table of predictable sequences for testing purposes", kElementsSeq0);

  // clang-format off
  static constexpr DataElement kElementsSeq1[] = {
      {"time_",
       "Timestamp when the data record was collected.",
       types::DataType::TIME64NS,
       types::SemanticType::ST_NONE,
       types::PatternType::METRIC_COUNTER},
      {"x",
       "A sequence number.",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
      {"xmod8",
       "The value of x % 8.",
       types::DataType::INT64,
       types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
  };
  // clang-format on
  static constexpr std::string_view kSeq1TabletizationKey = "xmod8";
  static constexpr auto kSeq1Table = DataTableSchema(
      "sequence_generator1", "A tabletized table of predictable sequences for testing purposes",
      kElementsSeq1, kSeq1TabletizationKey);

  static constexpr auto kTables = MakeArray(kSeq0Table, kSeq1Table);
  static constexpr uint32_t kSeq0TableNum = SourceConnector::TableNum(kTables, kSeq0Table);
  static constexpr uint32_t kSeq1TableNum = SourceConnector::TableNum(kTables, kSeq1Table);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new SeqGenConnector(name));
  }

  void SetSeed(uint32_t seed) { rng_.seed(seed); }

  void ConfigureNumRowsPerGet(uint32_t min_num_rows, uint32_t max_num_rows) {
    num_rows_min_ = min_num_rows;
    num_rows_max_ = max_num_rows;
  }

  void ConfigureNumRowsPerGet(uint32_t num_rows) { ConfigureNumRowsPerGet(num_rows, num_rows); }

 protected:
  explicit SeqGenConnector(std::string_view name)
      : SourceConnector(name, kTables),
        table0_lin_seq_(1, 1),
        table0_mod10_seq_(10),
        table0_square_seq_(1, 0, 0),
        table0_pi_seq_(3.14159, 0),
        table1_lin_seq_(2, 2),
        table1_mod8_seq_(8),
        rng_(37) {}
  ~SeqGenConnector() override = default;

  Status InitImpl() override;

  void TransferDataImpl(ConnectorContext* ctx) override;

  Status StopImpl() override { return Status::OK(); }

 private:
  void TransferDataTable0(uint32_t num_records, DataTable* data_table);
  void TransferDataTable1(uint32_t num_records, DataTable* data_table);

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
}  // namespace px
