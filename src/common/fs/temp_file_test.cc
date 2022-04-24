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

#include <fstream>
#include <string>

#include "src/common/base/file.h"
#include "src/common/base/logging.h"
#include "src/common/fs/temp_file.h"
#include "src/common/testing/testing.h"

namespace px {
namespace fs {

TEST(TempFile, Basic) {
  std::unique_ptr<TempFile> tmpf = TempFile::Create();
  std::filesystem::path fpath = tmpf->path();

  ASSERT_OK(WriteFileFromString(fpath, "Some data"));
  ASSERT_OK_AND_EQ(ReadFileToString(fpath), "Some data");
}

TEST(TempFile, ShouldFail) {
  std::filesystem::path fpath;
  {
    std::unique_ptr<TempFile> tmpf = TempFile::Create();
    fpath = tmpf->path();
  }

  ASSERT_NOT_OK(WriteFileFromString(fpath, "Some data"));
}

TEST(TempFile, ReferenceKeepsFileAccessible) {
  std::ifstream fin;
  std::filesystem::path fpath;
  {
    std::unique_ptr<TempFile> tmpf = TempFile::Create();
    fpath = tmpf->path();
    ASSERT_OK(WriteFileFromString(fpath, "Some data"));
    fin = std::ifstream(fpath);
  }

  std::string line;
  std::getline(fin, line);

  EXPECT_EQ(line, "Some data");
}

}  // namespace fs
}  // namespace px
