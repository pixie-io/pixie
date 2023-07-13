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

constexpr std::string_view kTestGoLittleEndiani386BinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go1_13_i386_binary";

constexpr std::string_view kTestGoLittleEndianBinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go_1_17_binary";

constexpr std::string_view kTestGoBinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go_1_19_binary";

// The "endian agnostic" case refers to where the Go version data is varint encoded
// directly within the buildinfo header. See the following reference for more details.
// https://github.com/golang/go/blob/1dbbafc70fd3e2c284469ab3e0936c1bb56129f6/src/debug/buildinfo/buildinfo.go#L184C16-L184C16
TEST(ReadGoBuildVersionTest, BuildinfoEndianAgnostic) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(std::string version, ReadGoBuildVersion(elf_reader.get()));
  EXPECT_THAT(version, StrEq("go1.19.10"));
}

TEST(ReadGoBuildVersionTest, BuildinfoLittleEndian) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoLittleEndianBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(std::string version, ReadGoBuildVersion(elf_reader.get()));
  EXPECT_THAT(version, StrEq("go1.17.13"));
}

TEST(ReadGoBuildVersionTest, BuildinfoLittleEndiani386) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoLittleEndiani386BinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(std::string version, ReadGoBuildVersion(elf_reader.get()));
  EXPECT_THAT(version, StrEq("go1.13.15"));
}

TEST(IsGoExecutableTest, WorkingOnBasicGoBinary) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  EXPECT_TRUE(IsGoExecutable(elf_reader.get()));
}

TEST(ElfGolangItableTest, ExtractInterfaceTypes) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoBinaryPath);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(const auto interfaces, ExtractGolangInterfaces(elf_reader.get()));

  // Check for `bazel coverage` so we can bypass the final checks.
  // Note that we still get accurate coverage metrics, because this only skips the final check.
  // Ideally, we'd get bazel to deterministically build test_go_binary,
  // but it's not easy to tell bazel to use a different config for just one target.
#ifdef PL_COVERAGE
  LOG(INFO) << "Whoa...`bazel coverage` is messaging with test_go_binary. Shame on you bazel. "
               "Ending this test early.";
  return;
#else
  EXPECT_THAT(
      interfaces,
      UnorderedElementsAre(
          Pair("error",
               UnorderedElementsAre(
                   Field(&IntfImplTypeInfo::type_name, "main.IntStruct"),
                   Field(&IntfImplTypeInfo::type_name, "*errors.errorString"),
                   Field(&IntfImplTypeInfo::type_name, "*io/fs.PathError"),
                   Field(&IntfImplTypeInfo::type_name, "*internal/poll.DeadlineExceededError"),
                   Field(&IntfImplTypeInfo::type_name, "internal/poll.errNetClosing"),
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
#endif
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
