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

#include <array>
#include <limits>

#include "src/stirling/utils/index_sorted_vector.h"

namespace px {
namespace stirling {
namespace utils {

TEST(SortedIndexes, Basic) {
  std::vector<int> data = {2, 0, 4, 10, 8, 6};
  std::vector<size_t> sort_indexes = SortedIndexes(data);

  EXPECT_EQ(sort_indexes, (std::vector<size_t>{1, 0, 2, 5, 4, 3}));
}

TEST(SplitSortedVector, Basic) {
  // Corresponds to {0, 2, 4, 6, 8, 10} after applying sort_indexes
  std::vector<int> data = {2, 0, 4, 10, 8, 6};
  std::vector<size_t> sort_indexes = {1, 0, 2, 5, 4, 3};

  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {0, 8}), (std::array<size_t, 2>{0, 4}));
  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {0, 9}), (std::array<size_t, 2>{0, 5}));
  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {1, 7}), (std::array<size_t, 2>{1, 4}));
  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {4, 7}), (std::array<size_t, 2>{2, 4}));
  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {3, 8}), (std::array<size_t, 2>{2, 4}));
  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {3, 7}), (std::array<size_t, 2>{2, 4}));
  EXPECT_EQ(SplitSortedVector<2>(data, sort_indexes, {3, 20}), (std::array<size_t, 2>{2, 6}));

  EXPECT_EQ(SplitSortedVector<3>(data, sort_indexes, {3, 5, 7}), (std::array<size_t, 3>{2, 3, 4}));
  EXPECT_EQ(SplitSortedVector<3>(data, sort_indexes, {3, 5, 20}), (std::array<size_t, 3>{2, 3, 6}));
  EXPECT_EQ(SplitSortedVector<3>(data, sort_indexes, {-1, 5, 20}),
            (std::array<size_t, 3>{0, 3, 6}));
}

void CheckAgainstReferenceModel(std::vector<int> vec, int t1, int t2,
                                std::array<size_t, 2> split_positions) {
  int num_left = 0;
  int num_middle = 0;
  int num_right = 0;

  for (const auto& t : vec) {
    if (t < t1) {
      ++num_left;
    } else if (t < t2) {
      ++num_middle;
    } else {
      ++num_right;
    }
  }

  EXPECT_EQ(num_left, split_positions[0]);
  EXPECT_EQ(num_middle, split_positions[1] - split_positions[0]);
  EXPECT_EQ(num_right, vec.size() - split_positions[1]);
}

TEST(SplitSortedVector, ReferenceModelComparison) {
  // Corresponds to {0, 2, 4, 6, 8, 10} after applying sort_indexes
  std::vector<int> data = {2, 0, 4, 10, 8, 6};
  std::vector<size_t> sort_indexes = {1, 0, 2, 5, 4, 3};

  // Bounds for the loops to make sure we cover all cases.
  // Use -1 and +1 to cover out-of-bounds edge cases.
  const int kMinVal = -1;
  const int kMaxVal = *std::max_element(data.begin(), data.end()) + 1;

  std::array<int, 2> search_vals;
  for (int i = kMinVal; i < kMaxVal; ++i) {
    search_vals[0] = i;
    for (int j = i; j < kMaxVal; ++j) {
      search_vals[1] = j;

      CheckAgainstReferenceModel(data, i, j, SplitSortedVector(data, sort_indexes, search_vals));
    }

    // Also test a really out-of-bounds case.
    int j = std::numeric_limits<int>::max();
    search_vals[1] = j;
    CheckAgainstReferenceModel(data, i, j, SplitSortedVector(data, sort_indexes, search_vals));
  }
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
