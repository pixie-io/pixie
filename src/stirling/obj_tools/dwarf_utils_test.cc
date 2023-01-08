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

#include "src/stirling/obj_tools/dwarf_utils.h"

#include <memory>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/obj_tools/dwarf_reader.h"

namespace px {
namespace stirling {
namespace obj_tools {

using ::testing::IsEmpty;
using ::testing::SizeIs;

constexpr std::string_view kCppBinary = "src/stirling/obj_tools/testdata/cc/test_exe_/test_exe";

class DwarfReaderTest : public ::testing::Test {
 protected:
  DwarfReaderTest() : kCppBinaryPath(px::testing::BazelRunfilePath(kCppBinary)) {}

  const std::string kCppBinaryPath;
};

TEST_F(DwarfReaderTest, GetMatchingDIEs) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<DwarfReader> dwarf_reader,
                       DwarfReader::CreateIndexingAll(kCppBinaryPath));

  std::vector<llvm::DWARFDie> dies;
  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("foo"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_variable);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("non-existent-name"), IsEmpty());

  ASSERT_OK_AND_ASSIGN(dies, dwarf_reader->GetMatchingDIEs("ABCStruct32"));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);

  EXPECT_OK_AND_THAT(dwarf_reader->GetMatchingDIEs("ABCStruct32", llvm::dwarf::DW_TAG_member),
                     IsEmpty());

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("px::testing::Foo::Bar", llvm::dwarf::DW_TAG_subprogram));
  ASSERT_THAT(dies, SizeIs(1));
  EXPECT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_subprogram);
  // Although the DIE does not have name attribute, DWARFDie::getShortName() walks
  // DW_AT_specification attribute to find the name.
  EXPECT_EQ(GetShortName(dies[0]), "Bar");
  EXPECT_THAT(std::string(GetLinkageName(dies[0])), ::testing::StrEq("_ZNK2px7testing3Foo3BarEi"));

  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("px::testing::Foo::Bar", "this"), 8);
  EXPECT_OK_AND_EQ(dwarf_reader->GetArgumentTypeByteSize("px::testing::Foo::Bar", "i"), 4);

  ASSERT_OK_AND_ASSIGN(
      dies, dwarf_reader->GetMatchingDIEs("ABCStruct32", llvm::dwarf::DW_TAG_structure_type));
  ASSERT_THAT(dies, SizeIs(1));
  ASSERT_EQ(dies[0].getTag(), llvm::dwarf::DW_TAG_structure_type);
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
