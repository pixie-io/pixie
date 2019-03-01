#pragma once
#include <memory>
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
};
}  // namespace exec
}  // namespace carnot
}  // namespace pl
