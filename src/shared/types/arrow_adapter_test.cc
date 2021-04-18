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
