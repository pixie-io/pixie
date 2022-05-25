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

#include <sys/types.h>
#include <sys/wait.h>
#include <filesystem>
#include <fstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <absl/strings/substitute.h>

// Uncomment to show the ASAN bug.
// #define DEMONSTRATE_ASAN_BUG

// This test shows an ASAN bug, where an exception is thrown while reading /proc/<pid>/stat.  In
// particular, the /proc/<pid>/stat file is opened, but the pid dies before the file is read. This
// bug also only affect C++ APIs, fgets() wont trigger the same ASAN exception.  This bug also wont
// happen when reading normal files (non-proc files), i.e., using std::getline() on normal files
// when it's deleted, won't result in an ASAN exception.
//
// This does not cause an issue in normal (non-ASAN) builds, where an error code is returned,
// which we then turn into a Status error. But, for some reason, with ASAN builds,
// the function throws an exception, which then causes ASAN to crash.
//
// In short:
//  bazel run --config=asan //src/common/system:proc_parser_bug_test  <--- Crashes
//  bazel run //src/common/system:proc_parser_bug_test                <--- Passes
//
// This is not our bug, and is hard (impossible?) for us to work around, as a PID could die at any
// moment.
//
// There does appear to be a related bug in libstdc++ for non-ASAN builds, which has been fixed:
//  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53984
//
// It is not an issue with regular files, as the process of opening a file guarantees that the file
// can still be read, even if it is deleted off the filesystem. Linux holds the file open via the
// FD, because there is a reference to it.
//
// The ASAN stack trace is included below:
//
// clang-format off
// NOLINT ==1875408==AddressSanitizer CHECK failed: /llvm_all/llvm-project/compiler-rt/lib/asan/asan_interceptors.cpp:350 "((__interception::real__Unwind_RaiseException)) != (0)" (0x0, 0x0)
// NOLINT    #0 0x7c59fe in __asan::AsanCheckFailed(char const*, int, char const*, unsigned long long, unsigned long long) /llvm_all/llvm-project/compiler-rt/lib/asan/asan_rtl.cpp:73:5
// NOLINT    #1 0x7d9faf in __sanitizer::CheckFailed(char const*, int, char const*, unsigned long long, unsigned long long) /llvm_all/llvm-project/compiler-rt/lib/sanitizer_common/sanitizer_termination.cpp:78:5
// NOLINT    #2 0x7a86b9 in __interceptor__Unwind_RaiseException /llvm_all/llvm-project/compiler-rt/lib/asan/asan_interceptors.cpp:350:3
// NOLINT    #3 0x16be00b in __cxa_throw (/home/oazizi/.cache/bazel/_bazel_oazizi/b26a4188b901df362a314e6f6307b112/execroot/pl/bazel-out/k8-dbg/bin/src/common/system/proc_parser_bug_test+0x16be00b)
// NOLINT    #4 0x171b193 in std::__throw_ios_failure(char const*, int) (/home/oazizi/.cache/bazel/_bazel_oazizi/b26a4188b901df362a314e6f6307b112/execroot/pl/bazel-out/k8-dbg/bin/src/common/system/proc_parser_bug_test+0x171b193)
// NOLINT    #5 0x17103dc in std::basic_filebuf<char, std::char_traits<char> >::underflow() (/home/oazizi/.cache/bazel/_bazel_oazizi/b26a4188b901df362a314e6f6307b112/execroot/pl/bazel-out/k8-dbg/bin/src/common/system/proc_parser_bug_test+0x17103dc)
// NOLINT    #6 0x16cadf9 in std::basic_istream<char, std::char_traits<char> >& std::getline<char, std::char_traits<char>, std::allocator<char> >(std::basic_istream<char, std::char_traits<char> >&, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >&, char) (/home/oazizi/.cache/bazel/_bazel_oazizi/b26a4188b901df362a314e6f6307b112/execroot/pl/bazel-out/k8-dbg/bin/src/common/system/proc_parser_bug_test+0x16cadf9)
// NOLINT    #7 0x7ede72 in GetPIDStartTimeTicksProxy(std::filesystem::__cxx11::path const&) /proc/self/cwd/src/common/system/proc_parser_bug_test.cc:70:3
// NOLINT    #8 0x7ee97c in GetPIDStartTimeTicks_ReproduceAsanBug_Test::TestBody() /proc/self/cwd/src/common/system/proc_parser_bug_test.cc:80:3
// NOLINT    #9 0xa89c93 in void testing::internal::HandleSehExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2433:10
// NOLINT    #10 0xa317b8 in void testing::internal::HandleExceptionsInMethodIfSupported<testing::Test, void>(testing::Test*, void (testing::Test::*)(), char const*) /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2469:14
// NOLINT    #11 0x9e3568 in testing::Test::Run() /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2508:5
// NOLINT    #12 0x9e5d06 in testing::TestInfo::Run() /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2684:11
// NOLINT    #13 0x9e79fb in testing::TestSuite::Run() /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2816:28
// NOLINT    #14 0xa10428 in testing::internal::UnitTestImpl::RunAllTests() /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:5338:44
// NOLINT    #15 0xa99233 in bool testing::internal::HandleSehExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2433:10
// NOLINT    #16 0xa3d01d in bool testing::internal::HandleExceptionsInMethodIfSupported<testing::internal::UnitTestImpl, bool>(testing::internal::UnitTestImpl*, bool (testing::internal::UnitTestImpl::*)(), char const*) /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:2469:14
// NOLINT    #17 0xa0ef3b in testing::UnitTest::Run() /proc/self/cwd/external/com_google_googletest/googletest/src/gtest.cc:4925:10
// NOLINT    #18 0x9a8479 in RUN_ALL_TESTS() /proc/self/cwd/external/com_google_googletest/googletest/include/gtest/gtest.h:2473:46
// NOLINT    #19 0x9a8230 in main /proc/self/cwd/src/common/testing/test_main.cc:7:16
// NOLINT    #20 0x7f15c6fa70b2 in __libc_start_main (/lib/x86_64-linux-gnu/libc.so.6+0x270b2)
// NOLINT    #21 0x742ecd in _start (/home/oazizi/.cache/bazel/_bazel_oazizi/b26a4188b901df362a314e6f6307b112/execroot/pl/bazel-out/k8-dbg/bin/src/common/system/proc_parser_bug_test+0x742ecd)
// clang-format on

void GetPIDStartTimeTicksProxy(const std::filesystem::path& proc_pid_path) {
  const std::filesystem::path proc_pid_stat_path = proc_pid_path / "stat";
  const std::string fpath = proc_pid_stat_path.string();

  std::ifstream ifs;
  ifs.open(fpath);
  ASSERT_TRUE(ifs) << absl::Substitute("Could not open file $0", fpath);

  // Wait for the child process to die.
  wait(NULL);

  // Now try to read /proc/<pid>/stat for the child process, which won't exist.
  std::string line;
  ASSERT_FALSE(std::getline(ifs, line))
      << absl::Substitute("Could not get line from file $0", fpath);
}

void GetPIDStartTimeTicksProxyWorkaround(const std::filesystem::path& proc_pid_path) {
  const std::filesystem::path proc_pid_stat_path = proc_pid_path / "stat";
  const std::string fpath = proc_pid_stat_path.string();

  std::FILE* fp = std::fopen(fpath.c_str(), "r");

  // Wait for the child process to die.
  wait(NULL);

  // Now try to read /proc/<pid>/stat for the child process.
  std::string line;
  line.resize(120);
  char* result = std::fgets(line.data(), line.size(), fp);

  // Turns out this sometimes succeeds and sometimes fails, due to some unknown race with Linux.
  // The important thing, however, is that we don't cause an ASAN crash.
  EXPECT_THAT(result, ::testing::AnyOf(nullptr, ::testing::Not(nullptr)));
}

class ASANBugReproTest : public ::testing::Test {
 protected:
  void SetUp() {
    int child_pid = fork();
    if (child_pid == 0) {
      sleep(1);
      exit(1);
    }
    proc_pid_stat_path_ = "/proc/" + std::to_string(child_pid);
  }

  std::filesystem::path proc_pid_stat_path_;
};

TEST_F(ASANBugReproTest, OriginalBuggyOnASAN) {
#if !defined(PL_CONFIG_ASAN) || defined(DEMONSTRATE_ASAN_BUG)
  ASSERT_NO_FATAL_FAILURE(GetPIDStartTimeTicksProxy(proc_pid_stat_path_));
#endif
}

TEST_F(ASANBugReproTest, Workaround) {
  ASSERT_NO_FATAL_FAILURE(GetPIDStartTimeTicksProxyWorkaround(proc_pid_stat_path_));
}
