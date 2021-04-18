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

#include <absl/strings/str_cat.h>
#include <gtest/gtest.h>

#include "src/common/base/file.h"
#include "src/common/testing/testing.h"

namespace px {

using ::px::testing::TempDir;

TEST(FileUtils, WriteThenRead) {
  std::string write_val = R"(This is a a file content.
It has two lines.)";

  TempDir tmp_dir;
  std::filesystem::path test_file = tmp_dir.path() / "file";

  EXPECT_OK(WriteFileFromString(test_file, write_val));
  std::string read_val = FileContentsOrDie(test_file);
  EXPECT_EQ(read_val, write_val);
}

}  // namespace px
