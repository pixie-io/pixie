#pragma once
#include <memory>
#include <utility>
#include <vector>

#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/exec/table.h"

namespace pl {
namespace carnot {
namespace exec {

class CarnotTestUtils {
 public:
  CarnotTestUtils() {}
  static std::shared_ptr<exec::Table> TestTable() {
    auto descriptor =
        std::vector<udf::UDFDataType>({types::DataType::FLOAT64, types::DataType::INT64});
    exec::RowDescriptor rd = exec::RowDescriptor(descriptor);

    auto table = std::make_shared<exec::Table>(rd);

    auto col1 = std::make_shared<exec::Column>(udf::UDFDataType::FLOAT64, "col1");
    std::vector<udf::Float64Value> col1_in1 = {0.5, 1.2, 5.3};
    std::vector<udf::Float64Value> col1_in2 = {0.1, 5.1};
    PL_CHECK_OK(col1->AddBatch(udf::ToArrow(col1_in1, arrow::default_memory_pool())));
    PL_CHECK_OK(col1->AddBatch(udf::ToArrow(col1_in2, arrow::default_memory_pool())));

    auto col2 = std::make_shared<exec::Column>(udf::UDFDataType::INT64, "col2");
    std::vector<udf::Int64Value> col2_in1 = {1, 2, 3};
    std::vector<udf::Int64Value> col2_in2 = {5, 6};
    PL_CHECK_OK(col2->AddBatch(udf::ToArrow(col2_in1, arrow::default_memory_pool())));
    PL_CHECK_OK(col2->AddBatch(udf::ToArrow(col2_in2, arrow::default_memory_pool())));

    PL_CHECK_OK(table->AddColumn(col1));
    PL_CHECK_OK(table->AddColumn(col2));

    return table;
  }

  static const std::vector<int64_t> big_test_col1;
  static const std::vector<double> big_test_col2;
  static const std::vector<int64_t> big_test_col3;

  static std::shared_ptr<exec::Table> BigTestTable() {
    auto descriptor = std::vector<udf::UDFDataType>(
        {types::DataType::TIME64NS, types::DataType::FLOAT64, types::DataType::INT64});
    exec::RowDescriptor rd = exec::RowDescriptor(descriptor);

    auto table = std::make_shared<exec::Table>(rd);
    std::vector<std::pair<int64_t, int64_t>> split_idx({{0, 3}, {3, 5}, {5, 8}});

    auto col1 = std::make_shared<exec::Column>(udf::UDFDataType::TIME64NS, "time_");
    auto col2 = std::make_shared<exec::Column>(udf::UDFDataType::FLOAT64, "col2");
    auto col3 = std::make_shared<exec::Column>(udf::UDFDataType::INT64, "col3");
    for (const auto& pair : split_idx) {
      std::vector<udf::Int64Value> col1_batch(big_test_col1.begin() + pair.first,
                                              big_test_col1.begin() + pair.second);
      EXPECT_OK(col1->AddBatch(udf::ToArrow(col1_batch, arrow::default_memory_pool())));

      std::vector<udf::Float64Value> col2_batch(big_test_col2.begin() + pair.first,
                                                big_test_col2.begin() + pair.second);
      EXPECT_OK(col2->AddBatch(udf::ToArrow(col2_batch, arrow::default_memory_pool())));

      std::vector<udf::Int64Value> col3_batch(big_test_col3.begin() + pair.first,
                                              big_test_col3.begin() + pair.second);
      EXPECT_OK(col3->AddBatch(udf::ToArrow(col3_batch, arrow::default_memory_pool())));
    }
    EXPECT_OK(table->AddColumn(col1));
    EXPECT_OK(table->AddColumn(col2));
    EXPECT_OK(table->AddColumn(col3));

    return table;
  }
};

const std::vector<int64_t> CarnotTestUtils::big_test_col1({1, 2, 3, 5, 6, 8, 9, 11});
const std::vector<double> CarnotTestUtils::big_test_col2({0.5, 1.2, 5.3, 0.1, 5.1, 5.2, 0.1, 7.3});
const std::vector<int64_t> CarnotTestUtils::big_test_col3({6, 2, 12, 5, 60, 56, 12, 13});
}  // namespace exec
}  // namespace carnot
}  // namespace pl
