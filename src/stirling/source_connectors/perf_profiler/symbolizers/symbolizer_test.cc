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

#include <set>

#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/testing.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/bcc_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/caching_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/elf_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/java_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/testing/testing.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/symbolization.h"
#include "src/stirling/utils/proc_tracker.h"

namespace test {
// foo() & bar() are not used directly, but in this test,
// we will find their symbols using the device under test, the symbolizer.
void foo() { LOG(INFO) << "foo()."; }
void bar() { LOG(INFO) << "bar()."; }
}  // namespace test

const uintptr_t kFooAddr = reinterpret_cast<uintptr_t>(&test::foo);
const uintptr_t kBarAddr = reinterpret_cast<uintptr_t>(&test::bar);

namespace px {
namespace stirling {

using ::px::stirling::profiler::testing::GetAgentLibsFlagValueForTesting;
using ::px::stirling::profiler::testing::GetPxJattachFlagValueForTesting;
using ::px::testing::BazelBinTestFilePath;

template <typename TSymbolizer>
class SymbolizerTest : public ::testing::Test {
 public:
  SymbolizerTest() = default;
  void SetUp() override { ASSERT_OK_AND_ASSIGN(symbolizer_, TSymbolizer::Create()); }
  std::unique_ptr<Symbolizer> symbolizer_;
};

using ElfSymbolizerTest = SymbolizerTest<ElfSymbolizer>;
using BCCSymbolizerTest = SymbolizerTest<BCCSymbolizer>;

using SymbolizerTypes = ::testing::Types<ElfSymbolizer, BCCSymbolizer>;
TYPED_TEST_SUITE(SymbolizerTest, SymbolizerTypes);

TYPED_TEST(SymbolizerTest, UserSymbols) {
  // We will use our self pid for symbolizing symbols from within this process.
  struct upid_t this_upid;
  this_upid.pid = static_cast<uint32_t>(getpid());
  this_upid.start_time_ticks = 0;

  auto symbolize = this->symbolizer_->GetSymbolizerFn(this_upid);

  EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
  EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
  EXPECT_EQ(symbolize(2), std::string("0x0000000000000002"));
}

TEST_F(BCCSymbolizerTest, KernelSymbols) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer, BCCSymbolizer::Create());

  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(const uint64_t kaddr, GetKernelSymAddr(kSymbolName));

  auto symbolize = symbolizer_->GetSymbolizerFn(profiler::kKernelUPID);
  EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
}

TEST_F(BCCSymbolizerTest, JavaSymbols) {
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  PL_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  SubProcess fake_java_proc;
  const std::filesystem::path fake_java_bin_path =
      BazelBinTestFilePath("src/stirling/source_connectors/perf_profiler/testing/java/java");
  ASSERT_TRUE(fs::Exists(fake_java_bin_path)) << fake_java_bin_path.string();
  ASSERT_OK(fake_java_proc.Start({fake_java_bin_path}));
  const uint32_t child_pid = fake_java_proc.child_pid();
  const uint64_t start_time_ns = 0;
  const struct upid_t child_upid = {{child_pid}, start_time_ns};

  symbolizer->IterationPreTick();
  symbolizer->GetSymbolizerFn(child_upid);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  ASSERT_TRUE(symbolizer->Uncacheable(child_upid)) << "Should have found symbol file by now.";
  auto symbolize = symbolizer->GetSymbolizerFn(child_upid);

  // The address ranges (100 to 199, 200 to 299, etc.) and the expected symbols are expected
  // based on the symbol file written by our fake "java" process; i.e. the fake "java" process
  // serves two purposes: 1. To trigger the java symbolization logic by being named "java" and
  // 2. To write a symbol file with known symbols and address ranges.
  for (uint32_t i = 100; i < 200; ++i) {
    constexpr std::string_view kExpected = "[j] int qux.foo::doStuff(foo.bar.TheStuff, boolean)";
    EXPECT_EQ(std::string(kExpected), std::string(symbolize(i)));
  }
  for (uint32_t i = 200; i < 300; ++i) {
    constexpr std::string_view kExpected =
        "[j] void foo.bar.baz.qux::doNothing(byte, boolean, int, long, float, double)";
    EXPECT_EQ(kExpected, symbolize(i));
  }
  for (uint32_t i = 300; i < 400; ++i) {
    constexpr std::string_view kExpected = "[j] byte foo.qux::compute(boolean)";
    EXPECT_EQ(kExpected, symbolize(i));
  }
  for (uint32_t i = 400; i < 500; ++i) {
    const auto expected = absl::StrFormat("0x%016llx", i);
    EXPECT_EQ(expected, symbolize(i));
  }
  for (uint32_t i = 500; i < 600; ++i) {
    constexpr std::string_view kExpected = "[j] long qux.foo::compute(double)";
    EXPECT_EQ(kExpected, symbolize(i));
  }
  ASSERT_OK_AND_ASSIGN(const auto artifacts_path, java::ResolveHostArtifactsPath(child_upid));
  EXPECT_TRUE(fs::Exists(artifacts_path));
  symbolizer->DeleteUPID(child_upid);
  EXPECT_FALSE(fs::Exists(artifacts_path));
}

// Expect that Java symbolization agents will not be injected after disabling.
TEST_F(BCCSymbolizerTest, DisableJavaSymbols) {
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  PL_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  const std::filesystem::path java_app_path =
      BazelBinTestFilePath("src/stirling/source_connectors/perf_profiler/testing/java/fib");
  ASSERT_TRUE(fs::Exists(java_app_path)) << java_app_path.string();
  ASSERT_TRUE(fs::Exists(FLAGS_stirling_profiler_px_jattach_path))
      << FLAGS_stirling_profiler_px_jattach_path;
  SubProcess java_proc_0;
  ASSERT_OK(java_proc_0.Start({java_app_path}));
  constexpr uint64_t start_time_ns = 0;
  const uint32_t child_pid_0 = java_proc_0.child_pid();
  const struct upid_t child_upid_0 = {{child_pid_0}, start_time_ns};

  symbolizer->IterationPreTick();
  symbolizer->GetSymbolizerFn(child_upid_0);
  std::this_thread::sleep_for(std::chrono::milliseconds{500});

  ASSERT_TRUE(symbolizer->Uncacheable(child_upid_0)) << "Should have found symbol file by now.";
  ASSERT_OK_AND_ASSIGN(const auto artifacts_path_0, java::ResolveHostArtifactsPath(child_upid_0));
  EXPECT_TRUE(fs::Exists(artifacts_path_0));

  // Disable java symbolization.
  FLAGS_stirling_profiler_java_symbols = false;

  // Disabling Java symbolization should not affect Java processes that we previously symbolized.
  EXPECT_TRUE(symbolizer->Uncacheable(child_upid_0)) << "Should have found symbol file by now.";
  EXPECT_TRUE(fs::Exists(artifacts_path_0));

  // Start a new Java sub-process.
  // We expect that it will not have a JVMTI symbolization agent injected.
  SubProcess java_proc_1;
  ASSERT_OK(java_proc_1.Start({java_app_path}));
  const uint32_t child_pid_1 = java_proc_1.child_pid();
  const struct upid_t child_upid_1 = {{child_pid_1}, start_time_ns};

  symbolizer->GetSymbolizerFn(child_upid_1);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  // Expect the symbols remain cacheable (non-Java).
  // Expect to *not* find the Java symbolization artifacts.
  EXPECT_FALSE(symbolizer->Uncacheable(child_upid_1));
  ASSERT_OK_AND_ASSIGN(const auto artifacts_path_1, java::ResolveHostArtifactsPath(child_upid_1));
  EXPECT_FALSE(fs::Exists(artifacts_path_1));

  // Expect that JVMTI agent injection tracking includes sub-proc-0 but not sub-proc-1.
  EXPECT_TRUE(JavaProfilingProcTracker::GetSingleton()->upids().contains(child_upid_0));
  EXPECT_FALSE(JavaProfilingProcTracker::GetSingleton()->upids().contains(child_upid_1));
}

// Test the symbolizer with caching enabled and disabled.
TEST_F(BCCSymbolizerTest, Caching) {
  // We will use our self pid for symbolizing symbols from within this process,
  // *and* we will trigger the kprobe that grabs a symbol from the kernel.
  const uint32_t pid = getpid();

  // Create a caching layer around our symbolizer.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer_uptr,
                       CachingSymbolizer::Create(std::move(symbolizer_)));
  CachingSymbolizer& symbolizer = *static_cast<CachingSymbolizer*>(symbolizer_uptr.get());

  const struct upid_t this_upid = {.pid = pid, .start_time_ticks = 0};

  // Lookup the addresses for the first time. These should be cache misses.
  // We are placing each symbol lookup into its own scope to force us to
  // "re-lookup" the pid symbolizer function from inside of the symbolize instance.
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 1);
    EXPECT_EQ(symbolizer.stat_hits(), 0);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 1);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 2);
    EXPECT_EQ(symbolizer.stat_hits(), 0);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }

  // Lookup the addresses a second time. We should get cache hits.
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 3);
    EXPECT_EQ(symbolizer.stat_hits(), 1);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }

  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 4);
    EXPECT_EQ(symbolizer.stat_hits(), 2);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }

  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(const uint64_t kaddr, GetKernelSymAddr(kSymbolName));

  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 5);
    EXPECT_EQ(symbolizer.stat_hits(), 2);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 6);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }

  // This will flush the caches, access count & hit count will remain the same.
  // We will lookup the symbols and again expect a miss then a hit.
  symbolizer.DeleteUPID(this_upid);
  EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 1);
  symbolizer.DeleteUPID(profiler::kKernelUPID);
  EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 0);

  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 7);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 1);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 8);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 2);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 9);
    EXPECT_EQ(symbolizer.stat_hits(), 3);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kFooAddr), "test::foo()");
    EXPECT_EQ(symbolizer.stat_accesses(), 10);
    EXPECT_EQ(symbolizer.stat_hits(), 4);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(kBarAddr), "test::bar()");
    EXPECT_EQ(symbolizer.stat_accesses(), 11);
    EXPECT_EQ(symbolizer.stat_hits(), 5);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(profiler::kKernelUPID);
    EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
    EXPECT_EQ(symbolizer.stat_accesses(), 12);
    EXPECT_EQ(symbolizer.stat_hits(), 6);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 3);
  }

  // Test the feature that converts "[UNKNOWN]" into 0x<addr>.
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(0x1234123412341234ULL), "0x1234123412341234");
    EXPECT_EQ(symbolizer.stat_accesses(), 13);
    EXPECT_EQ(symbolizer.stat_hits(), 6);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 4);
  }
  {
    auto symbolize = symbolizer.GetSymbolizerFn(this_upid);
    EXPECT_EQ(symbolize(0x1234123412341234ULL), "0x1234123412341234");
    EXPECT_EQ(symbolizer.stat_accesses(), 14);
    EXPECT_EQ(symbolizer.stat_hits(), 7);
    EXPECT_EQ(symbolizer.GetNumberOfSymbolsCached(), 4);
  }
}

// Expect that upids for Java processes (that we attempt to symbolize) are inserted to global set.
TEST_F(BCCSymbolizerTest, JavaProcessBeingTracked) {
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PL_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  PL_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  SubProcess fake_java_proc;
  const std::filesystem::path fake_java_bin_path =
      BazelBinTestFilePath("src/stirling/source_connectors/perf_profiler/testing/java/java");
  ASSERT_TRUE(fs::Exists(fake_java_bin_path));
  ASSERT_OK(fake_java_proc.Start({fake_java_bin_path}));
  const uint32_t child_pid = fake_java_proc.child_pid();

  system::ProcParser parser(system::Config::GetInstance());
  ASSERT_OK_AND_ASSIGN(const uint64_t start_time_ns, parser.GetPIDStartTimeTicks(child_pid));

  const struct upid_t child_upid = {{child_pid}, start_time_ns};
  symbolizer->IterationPreTick();
  symbolizer->GetSymbolizerFn(child_upid);
  EXPECT_TRUE(JavaProfilingProcTracker::GetSingleton()->upids().contains(child_upid));
}

}  // namespace stirling
}  // namespace px
