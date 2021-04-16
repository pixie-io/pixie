#include <gtest/gtest.h>

#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"

namespace px {
namespace types {

TEST(BinarySearchTest, find_index_greater_or_eq) {
  std::vector<types::Int64Value> col_rb1 = {1, 2, 5, 6, 9, 11};
  auto col_rb1_arrow = ToArrow(col_rb1, arrow::default_memory_pool());

  EXPECT_EQ(1, SearchArrowArrayGreaterThanOrEqual<types::DataType::INT64>(col_rb1_arrow.get(), 2));
  EXPECT_EQ(-1,
            SearchArrowArrayGreaterThanOrEqual<types::DataType::INT64>(col_rb1_arrow.get(), 12));
  EXPECT_EQ(2, SearchArrowArrayGreaterThanOrEqual<types::DataType::INT64>(col_rb1_arrow.get(), 4));
}

TEST(BinarySearchTest, find_index_lower_or_eq) {
  std::vector<types::Int64Value> col_rb1 = {1, 2, 5, 6, 6, 6, 9, 11};
  auto col_rb1_arrow = types::ToArrow(col_rb1, arrow::default_memory_pool());

  EXPECT_EQ(0, SearchArrowArrayLessThan<types::DataType::INT64>(col_rb1_arrow.get(), 2));
  EXPECT_EQ(-1, SearchArrowArrayLessThan<types::DataType::INT64>(col_rb1_arrow.get(), 0));
  EXPECT_EQ(-1, SearchArrowArrayLessThan<types::DataType::INT64>(col_rb1_arrow.get(), 1));
  EXPECT_EQ(3, SearchArrowArrayLessThan<types::DataType::INT64>(col_rb1_arrow.get(), 8));
}

}  // namespace types
}  // namespace px
