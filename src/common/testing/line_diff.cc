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

#include "src/common/testing/line_diff.h"

#include <iostream>

#include "src/common/base/base.h"

namespace px {
namespace testing {

namespace {

enum class Direction {
  kDefault,
  kDiag,
  kUp,
  kLeft,
};

struct LinePair {
  size_t lhs = 0;
  size_t rhs = 0;

  std::string ToString() const { return absl::Substitute("lhs: $0 rhs: $1", lhs, rhs); }
};

std::vector<LinePair> TraceLCS(const std::vector<std::vector<Direction>>& directions) {
  std::vector<LinePair> line_pairs;
  int i = directions.size() - 1;
  int j = directions.begin()->size() - 1;
  while (i >= 0 && j >= 0) {
    if (directions[i][j] == Direction::kDiag) {
      line_pairs.push_back({static_cast<size_t>(i - 1), static_cast<size_t>(j - 1)});
      --i;
      --j;
    } else if (directions[i][j] == Direction::kUp) {
      --i;
    } else {
      --j;
    }
  }
  return line_pairs;
}

// A dynamic programming solution to the longest common subsequence problem.
// One such example: https://github.com/ben093/Longest-Common-Subsequence/blob/master/h4.cpp
std::vector<LinePair> LCS(const std::vector<std::string>& lhs,
                          const std::vector<std::string>& rhs) {
  std::vector<std::vector<Direction>> directions(lhs.size() + 1,
                                                 std::vector<Direction>(rhs.size() + 1));

  std::vector<std::vector<int>> lcs_counters(lhs.size() + 1, std::vector<int>(rhs.size() + 1, 0));

  // Initialize default value for the dynamic programming memorization table.
  for (size_t i = 0; i <= lhs.size(); ++i) {
    for (size_t j = 0; j <= rhs.size(); ++j) {
      if (i == 0 || j == 0) {
        directions[i][j] = Direction::kDefault;
        lcs_counters[i][j] = 0;
      }
    }
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    for (size_t j = 0; j < rhs.size(); ++j) {
      if (lhs[i] == rhs[j]) {
        lcs_counters[i + 1][j + 1] = lcs_counters[i][j] + 1;
        directions[i + 1][j + 1] = Direction::kDiag;
      } else if (lcs_counters[i][j + 1] >= lcs_counters[i + 1][j]) {
        lcs_counters[i + 1][j + 1] = lcs_counters[i][j + 1];
        directions[i + 1][j + 1] = Direction::kUp;
      } else {
        lcs_counters[i + 1][j + 1] = lcs_counters[i + 1][j];
        directions[i + 1][j + 1] = Direction::kLeft;
      }
    }
  }

  std::vector<LinePair> lcs = TraceLCS(directions);
  std::reverse(lcs.begin(), lcs.end());

  return lcs;
}

}  // namespace

std::string Diff(const std::vector<std::string>& lhs, const std::vector<std::string>& rhs) {
  std::vector<std::string> diffs;
  size_t l = 0;
  size_t r = 0;
  for (const LinePair& line_pair : LCS(lhs, rhs)) {
    while (l < line_pair.lhs) {
      diffs.push_back(absl::StrCat("l:", lhs[l++]));
    }
    while (r < line_pair.rhs) {
      diffs.push_back(absl::StrCat("r:", rhs[r++]));
    }
    diffs.push_back(absl::StrCat("  ", lhs[line_pair.lhs]));
    l = line_pair.lhs + 1;
    r = line_pair.rhs + 1;
  }
  while (l < lhs.size()) {
    diffs.push_back(absl::StrCat("l:", lhs[l++]));
  }
  while (r < rhs.size()) {
    diffs.push_back(absl::StrCat("r:", rhs[r++]));
  }
  return absl::StrJoin(diffs, "\n");
}

std::string DiffLines(const std::string& lhs, const std::string& rhs, DiffPolicy policy) {
  switch (policy) {
    case DiffPolicy::kDefault: {
      std::vector<std::string> lhs_lines = absl::StrSplit(lhs, "\n");
      std::vector<std::string> rhs_lines = absl::StrSplit(rhs, "\n");
      return Diff(lhs_lines, rhs_lines);
    }
    case DiffPolicy::kIgnoreBlankLines: {
      std::vector<std::string> lhs_lines = absl::StrSplit(lhs, "\n", absl::SkipEmpty());
      std::vector<std::string> rhs_lines = absl::StrSplit(rhs, "\n", absl::SkipEmpty());
      return Diff(lhs_lines, rhs_lines);
    }
  }
  // GCC does not recognize that the above switch statement is exhaustive.
  return {};
}

}  // namespace testing
}  // namespace px
