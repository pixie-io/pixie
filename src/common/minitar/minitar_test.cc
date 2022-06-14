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

#include <filesystem>
#include <string>

#include "src/common/base/base.h"
#include "src/common/testing/testing.h"

#include "src/common/exec/exec.h"
#include "src/common/minitar/minitar.h"

using ::px::testing::BazelRunfilePath;
using ::testing::MatchesRegex;

TEST(Minitar, Extract) {
  // Tarball and a reference extracted version for comparison.
  std::filesystem::path tarball = BazelRunfilePath("src/common/minitar/testdata/a.tar.gz");
  std::filesystem::path ref = BazelRunfilePath("src/common/minitar/testdata/ref");

  ::px::tools::Minitar minitar(tarball);
  EXPECT_OK(minitar.Extract());

  std::string diff_cmd = absl::Substitute("diff -rs a $0", ref.string());
  const std::string kExpectedDiffResult =
      "Files a/bar and .*src/common/minitar/testdata/ref/bar are identical\n"
      "Files a/foo and .*src/common/minitar/testdata/ref/foo are identical\n";
  EXPECT_OK_AND_THAT(::px::Exec(diff_cmd), MatchesRegex(kExpectedDiffResult));
}

TEST(Minitar, ExtractTo) {
  // Tarball and a reference extracted version for comparison.
  std::filesystem::path tarball = BazelRunfilePath("src/common/minitar/testdata/a.tar.gz");
  std::filesystem::path ref = BazelRunfilePath("src/common/minitar/testdata/ref");

  ::px::tools::Minitar minitar(tarball);
  EXPECT_OK(minitar.Extract("src/common/minitar/testdata"));

  std::string diff_cmd =
      absl::Substitute("diff -rs src/common/minitar/testdata/a $0", ref.string());
  const std::string kExpectedDiffResult =
      "Files src/common/minitar/testdata/a/bar and .*src/common/minitar/testdata/ref/bar are "
      "identical\n"
      "Files src/common/minitar/testdata/a/foo and .*src/common/minitar/testdata/ref/foo are "
      "identical\n";
  EXPECT_OK_AND_THAT(::px::Exec(diff_cmd), MatchesRegex(kExpectedDiffResult));
}
