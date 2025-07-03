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
#include <tuple>
#include <utility>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace obj_tools {

using ::testing::Field;
using ::testing::Matcher;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

constexpr std::string_view kTestGoLittleEndiani386BinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go1_13_i386_binary";

constexpr std::string_view kTestGo1_11BinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go_1_11_binary";

constexpr std::string_view kTestGoLittleEndianBinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go_1_17_binary";

constexpr std::string_view kTestGoWithModulesBinaryPath =
    "src/stirling/obj_tools/testdata/go/test_buildinfo_with_mods";

constexpr std::string_view kTestGoBinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go_1_19_binary";
constexpr std::string_view kTestGo1_21BinaryPath =
    "src/stirling/obj_tools/testdata/go/test_go_1_21_binary";

// The "endian agnostic" case refers to where the Go version data is varint encoded
// directly within the buildinfo header. See the following reference for more details.
// https://github.com/golang/go/blob/1dbbafc70fd3e2c284469ab3e0936c1bb56129f6/src/debug/buildinfo/buildinfo.go#L184C16-L184C16
TEST(ReadGoBuildInfoTest, BuildinfoEndianAgnostic) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(auto pair, ReadGoBuildInfo(elf_reader.get()));
  auto version = pair.first;
  EXPECT_THAT(version, StrEq("1.19.13"));
}

TEST(ReadGoBuildInfoTest, BuildinfoLittleEndian) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoLittleEndianBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(auto pair, ReadGoBuildInfo(elf_reader.get()));
  auto version = pair.first;
  EXPECT_THAT(version, StrEq("1.17.13"));
}

// These tests are modeled off of upstream's
// https://github.com/golang/go/blob/93fb2c90740aef00553c9ce6a7cd4578c2469675/src/runtime/debug/mod_test.go#L23
TEST(ReadGoBuildInfoTest, BuildinfoPackageBuiltOutsideModule) {
  const std::string kBinContent =
      "path\trsc.io/fortune\n"
      "mod\trsc.io/fortune\tv1.0.0";
  auto build_info_s = ReadModInfo(kBinContent);
  EXPECT_OK(build_info_s);

  auto build_info = build_info_s.ConsumeValueOrDie();
  EXPECT_EQ(build_info.path, "rsc.io/fortune");
  EXPECT_EQ(build_info.main.path, "rsc.io/fortune");
  EXPECT_EQ(build_info.main.version, "v1.0.0");
}

TEST(ReadGoBuildInfoTest, BuildinfoPackageBuiltStdlib) {
  const std::string kBinContent = "path\tcmd/test2json";
  auto build_info_s = ReadModInfo(kBinContent);
  EXPECT_OK(build_info_s);
  auto build_info = build_info_s.ConsumeValueOrDie();
  EXPECT_EQ(build_info.path, "cmd/test2json");
}

TEST(ReadGoBuildInfoTest, BuildinfoPackageBuiltInsideModule) {
  const std::string kBinContent =
      "go\t1.18\n"
      "path\texample.com/m\n"
      "mod\texample.com/m\t(devel)\n"
      "build\t-compiler=gc";
  auto build_info_s = ReadModInfo(kBinContent);
  EXPECT_OK(build_info_s);

  auto build_info = build_info_s.ConsumeValueOrDie();
  EXPECT_EQ(build_info.path, "example.com/m");
  EXPECT_EQ(build_info.main.path, "example.com/m");
  EXPECT_EQ(build_info.main.version, "(devel)");
  EXPECT_EQ(build_info.main.replace, nullptr);
  EXPECT_EQ(build_info.deps.size(), 0);
}

TEST(ReadGoBuildInfoTest, BuildinfoWithModules) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoWithModulesBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(auto pair, ReadGoBuildInfo(elf_reader.get()));
  auto version = pair.first;
  EXPECT_THAT(version, StrEq("1.23.0"));

  auto& build_info = pair.second;
  // Validate main module path.
  EXPECT_THAT(build_info.path,
              StrEq("go.opentelemetry.io/auto/internal/tools/inspect/cmd/offsetgen"));

  // Validate main module metadata.
  EXPECT_THAT(build_info.main.path, StrEq("go.opentelemetry.io/auto/internal/tools"));
  EXPECT_THAT(build_info.main.version, StrEq("(devel)"));
  EXPECT_EQ(build_info.main.replace, nullptr);

  // Validate module dependencies.
  EXPECT_THAT(build_info.deps,
              UnorderedElementsAre(
                  Field(&Module::path, StrEq("github.com/Masterminds/semver/v3")),
                  Field(&Module::path, StrEq("github.com/cilium/ebpf")),
                  Field(&Module::path, StrEq("github.com/distribution/reference")),
                  Field(&Module::path, StrEq("github.com/docker/docker")),
                  Field(&Module::path, StrEq("github.com/docker/go-connections")),
                  Field(&Module::path, StrEq("github.com/docker/go-units")),
                  Field(&Module::path, StrEq("github.com/felixge/httpsnoop")),
                  Field(&Module::path, StrEq("github.com/go-logr/logr")),
                  Field(&Module::path, StrEq("github.com/go-logr/stdr")),
                  Field(&Module::path, StrEq("github.com/gogo/protobuf")),
                  Field(&Module::path, StrEq("github.com/moby/docker-image-spec")),
                  Field(&Module::path, StrEq("github.com/opencontainers/go-digest")),
                  Field(&Module::path, StrEq("github.com/opencontainers/image-spec")),
                  Field(&Module::path, StrEq("github.com/pkg/errors")),
                  Field(&Module::path, StrEq("go.opentelemetry.io/auto")),
                  Field(&Module::path, StrEq("go.opentelemetry.io/auto/sdk")),
                  Field(&Module::path, StrEq("go.opentelemetry.io/collector/pdata")),
                  Field(&Module::path,
                        StrEq("go.opentelemetry.io/contrib/instrumentation/net/http/otelhttp")),
                  Field(&Module::path, StrEq("go.opentelemetry.io/otel")),
                  Field(&Module::path, StrEq("go.opentelemetry.io/otel/metric")),
                  Field(&Module::path, StrEq("go.opentelemetry.io/otel/trace")),
                  Field(&Module::path, StrEq("go.uber.org/multierr")),
                  Field(&Module::path, StrEq("golang.org/x/arch")),
                  Field(&Module::path, StrEq("golang.org/x/net")),
                  Field(&Module::path, StrEq("golang.org/x/sync")),
                  Field(&Module::path, StrEq("golang.org/x/sys")),
                  Field(&Module::path, StrEq("golang.org/x/text")),
                  Field(&Module::path, StrEq("google.golang.org/genproto/googleapis/rpc")),
                  Field(&Module::path, StrEq("google.golang.org/grpc")),
                  Field(&Module::path, StrEq("google.golang.org/protobuf"))));

  // Validate replaced modules.
  EXPECT_THAT(build_info.deps,
              Contains(AllOf(Field(&Module::path, StrEq("go.opentelemetry.io/auto")),
                             Field(&Module::replace,
                                   Pointee(AllOf(Field(&Module::path, StrEq("../../")),
                                                 Field(&Module::version, StrEq("(devel)"))))))));

  EXPECT_THAT(build_info.deps,
              Contains(AllOf(Field(&Module::path, StrEq("go.opentelemetry.io/auto/sdk")),
                             Field(&Module::replace,
                                   Pointee(AllOf(Field(&Module::path, StrEq("../../sdk")),
                                                 Field(&Module::version, StrEq("(devel)"))))))));
}

TEST(ReadGoBuildInfoTest, BuildinfoLittleEndiani386) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoLittleEndiani386BinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(auto pair, ReadGoBuildInfo(elf_reader.get()));

  auto version = pair.first;
  EXPECT_THAT(version, StrEq("1.13.15"));

  auto& buildinfo = pair.second;
  EXPECT_THAT(buildinfo.path, StrEq("command-line-arguments"));
  EXPECT_THAT(buildinfo.main.path, StrEq("px.dev/pixie"));
  EXPECT_THAT(buildinfo.main.version, StrEq("(devel)"));
}

TEST(ReadGoBuildInfoTest, BuildVersionLittleEndiani386) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoLittleEndiani386BinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  auto result = ReadBuildVersion(elf_reader.get());

  EXPECT_FALSE(result.ok());
  EXPECT_THAT(result.msg(), ::testing::HasSubstr("Refusing to preallocate that much memory"));
}

TEST(ReadGoBuildInfoTest, BuildVersionGo1_11) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGo1_11BinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  ASSERT_OK_AND_ASSIGN(auto pair, ReadBuildVersion(elf_reader.get()));

  auto version = pair.first;
  EXPECT_THAT(version, StrEq("1.11.13"));
}

TEST(IsGoExecutableTest, WorkingOnBasicGoBinary) {
  const std::string kPath = px::testing::BazelRunfilePath(kTestGoBinaryPath);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));
  EXPECT_TRUE(IsGoExecutable(elf_reader.get()));
}

class ElfGolangItableTest
    : public ::testing::TestWithParam<std::tuple<
          std::string,
          Matcher<const std::vector<std::pair<std::string, std::vector<IntfImplTypeInfo>>>>>> {};

INSTANTIATE_TEST_SUITE_P(
    ElfGolangItableTestSuite, ElfGolangItableTest,
    ::testing::Values(
        std::make_tuple(
            kTestGo1_21BinaryPath,
            UnorderedElementsAre(
                Pair("fmt.State",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*fmt.pp"))),
                Pair("internal/bisect.Writer",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                "*internal/godebug.runtimeStderr"))),
                Pair("internal/reflectlite.Type",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                "internal/reflectlite.rtype"))),
                Pair("error",
                     UnorderedElementsAre(
                         Field(&IntfImplTypeInfo::type_name, "main.IntStruct"),
                         Field(&IntfImplTypeInfo::type_name, "*errors.errorString"),
                         Field(&IntfImplTypeInfo::type_name, "syscall.Errno"),
                         Field(&IntfImplTypeInfo::type_name, "*io/fs.PathError"),
                         Field(&IntfImplTypeInfo::type_name, "runtime.errorString"),
                         Field(&IntfImplTypeInfo::type_name, "internal/poll.errNetClosing"),
                         Field(&IntfImplTypeInfo::type_name,
                               "*internal/poll.DeadlineExceededError"),
                         Field(&IntfImplTypeInfo::type_name, "*internal/bisect.parseError"))),
                Pair("io.Writer",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*os.File"))),
                Pair("sort.Interface", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                                  "*internal/fmtsort.SortedMap"))),
                Pair("reflect.Type",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*reflect.rtype"))),
                Pair("math/rand.Source64", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                                      "*math/rand.fastSource"))),
                Pair("math/rand.Source", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                                    "*math/rand.lockedSource"),
                                                              Field(&IntfImplTypeInfo::type_name,
                                                                    "*math/rand.fastSource"))))),
        std::make_tuple(
            kTestGoBinaryPath,
            UnorderedElementsAre(
                Pair("error",
                     UnorderedElementsAre(
                         Field(&IntfImplTypeInfo::type_name, "main.IntStruct"),
                         Field(&IntfImplTypeInfo::type_name, "*errors.errorString"),
                         Field(&IntfImplTypeInfo::type_name, "*io/fs.PathError"),
                         Field(&IntfImplTypeInfo::type_name,
                               "*internal/poll.DeadlineExceededError"),
                         Field(&IntfImplTypeInfo::type_name, "internal/poll.errNetClosing"),
                         Field(&IntfImplTypeInfo::type_name, "runtime.errorString"),
                         Field(&IntfImplTypeInfo::type_name, "syscall.Errno"))),
                Pair("sort.Interface", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                                  "*internal/fmtsort.SortedMap"))),
                Pair("math/rand.Source", UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                                    "*math/rand.lockedSource"))),
                Pair("io.Writer",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*os.File"))),
                Pair("internal/reflectlite.Type",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name,
                                                "*internal/reflectlite.rtype"))),
                Pair("reflect.Type",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*reflect.rtype"))),
                Pair("fmt.State",
                     UnorderedElementsAre(Field(&IntfImplTypeInfo::type_name, "*fmt.pp")))))));

TEST_P(ElfGolangItableTest, ExtractInterfaceTypes) {
  const std::string kPath = px::testing::BazelRunfilePath(std::get<0>(GetParam()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ElfReader> elf_reader, ElfReader::Create(kPath));

  ASSERT_OK_AND_ASSIGN(const auto interfaces_map, ExtractGolangInterfaces(elf_reader.get()));
  std::vector<std::pair<std::string, std::vector<IntfImplTypeInfo>>> interfaces(
      interfaces_map.begin(), interfaces_map.end());

  // Check for `bazel coverage` so we can bypass the final checks.
  // Note that we still get accurate coverage metrics, because this only skips the final check.
  // Ideally, we'd get bazel to deterministically build test_go_binary,
  // but it's not easy to tell bazel to use a different config for just one target.

#ifdef PL_COVERAGE
  LOG(INFO) << "Whoa...`bazel coverage` is messaging with test_go_binary. Shame on you bazel. "
               "Ending this test early.";
  return;
#else
  EXPECT_THAT(interfaces, std::get<1>(GetParam()));
#endif
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace px
