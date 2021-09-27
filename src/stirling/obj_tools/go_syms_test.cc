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

#include "src/stirling/obj_tools/go_syms.h"

#include <memory>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace obj_tools {

using ::testing::StrEq;

TEST(ReadBuildVersionTest, WorkingOnBasicGoBinary) {
  const std::string kPath = px::testing::BazelBinTestFilePath(
      "src/stirling/obj_tools/testdata/dummy_go_binary_/dummy_go_binary");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(std::string version, ReadBuildVersion(elf_reader.get()));
  EXPECT_THAT(version, StrEq("go1.16"));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
