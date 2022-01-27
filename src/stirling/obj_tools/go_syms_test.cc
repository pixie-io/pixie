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

using ::testing::Field;
using ::testing::StrEq;

constexpr std::string_view kTestGoBinaryPath = "src/stirling/obj_tools/testdata/go/test_go_binary";

TEST(ReadBuildVersionTest, WorkingOnBasicGoBinary) {
  const std::string kPath = px::testing::BazelBinTestFilePath(kTestGoBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(std::string version, ReadBuildVersion(elf_reader.get()));
  EXPECT_THAT(version, StrEq("go1.16.12"));
}

TEST(IsGoExecutableTest, WorkingOnBasicGoBinary) {
  const std::string kPath = px::testing::BazelBinTestFilePath(kTestGoBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  EXPECT_TRUE(IsGoExecutable(elf_reader.get()));
}

TEST(ElfGolangItableTest, ExtractInterfaceTypes) {
  const std::string kPath = px::testing::BazelBinTestFilePath(kTestGoBinaryPath);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(const auto interfaces, ExtractGolangInterfaces(elf_reader.get()));

  EXPECT_THAT(
      interfaces,
      UnorderedElementsAre(
          Pair("error",
               UnorderedElementsAre(
                   Field(&IntfImplTypeInfo::type_name, "main.IntStruct"),
                   Field(&IntfImplTypeInfo::type_name, "*errors.errorString"),
                   Field(&IntfImplTypeInfo::type_name, "*io/fs.PathError"),
                   Field(&IntfImplTypeInfo::type_name, "*internal/poll.DeadlineExceededError"),
                   Field(&IntfImplTypeInfo::type_name, "runtime.errorString"),
                   Field(&IntfImplTypeInfo::type_name, "syscall.Errno"))),
          Pair("sort.Interface", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                            "*internal/fmtsort.SortedMap"))),
          Pair("math/rand.Source", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                              "*math/rand.lockedSource"))),
          Pair("io.Writer", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*os.File"))),
          Pair("internal/reflectlite.Type",
               UnorderedElementsAre(
                   Field(&IntfImplTypeInfo::type_name, "*internal/reflectlite.rtype"))),
          Pair("reflect.Type",
               UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*reflect.rtype"))),
          Pair("fmt.State", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*fmt.pp")))));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
