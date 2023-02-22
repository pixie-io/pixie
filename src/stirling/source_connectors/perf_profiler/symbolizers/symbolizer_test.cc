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

#include <sys/mount.h>

#include <gtest/gtest.h>

#include <set>

#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/test_utils/container_runner.h"
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

DEFINE_string(java_image_name, "none", "Java docker image for test cases.");

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
using ::px::testing::BazelRunfilePath;

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
}

TEST_F(BCCSymbolizerTest, KernelSymbols) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer, BCCSymbolizer::Create());

  std::string_view kSymbolName = "cpu_detect";
  ASSERT_OK_AND_ASSIGN(const uint64_t kaddr, GetKernelSymAddr(kSymbolName));

  auto symbolize = symbolizer_->GetSymbolizerFn(profiler::kKernelUPID);
  EXPECT_EQ(std::string(symbolize(kaddr)), kSymbolName);
}

// This test uses a "fake" Java binary that creates a pre-populated (canned) symbol file.
// Because the fake Java process is named "java" the process is categorized as Java.
// The symbolizer finds the pre-existing symbol file, and early exits the attach process.
// This test expects to find known symbols at known addresses based on the canned symbol file.
TEST_F(BCCSymbolizerTest, JavaSymbols) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  PX_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  SubProcess fake_java_proc;
  DEFER(fake_java_proc.Kill());
  const std::filesystem::path fake_java_bin_path =
      BazelRunfilePath("src/stirling/source_connectors/perf_profiler/testing/java/java");
  ASSERT_TRUE(fs::Exists(fake_java_bin_path)) << fake_java_bin_path.string();
  ASSERT_OK(fake_java_proc.Start({fake_java_bin_path}));
  const uint32_t child_pid = fake_java_proc.child_pid();
  const uint64_t start_time_ns = 0;
  const struct upid_t child_upid = {{child_pid}, start_time_ns};

  symbolizer->IterationPreTick();
  symbolizer->GetSymbolizerFn(child_upid);
  std::this_thread::sleep_for(std::chrono::milliseconds{500});

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
  const auto artifacts_path = java::AgentArtifactsPath(child_upid);
  EXPECT_TRUE(fs::Exists(artifacts_path));
  symbolizer->DeleteUPID(child_upid);
  EXPECT_FALSE(fs::Exists(artifacts_path));
}

// Expect that Java symbolization agents will not be injected after disabling.
TEST_F(BCCSymbolizerTest, DisableJavaSymbols) {
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  PX_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  const std::filesystem::path java_app_path =
      BazelRunfilePath("src/stirling/source_connectors/perf_profiler/testing/java/profiler_test");
  ASSERT_TRUE(fs::Exists(java_app_path)) << java_app_path.string();
  ASSERT_TRUE(fs::Exists(FLAGS_stirling_profiler_px_jattach_path))
      << FLAGS_stirling_profiler_px_jattach_path;
  SubProcess java_proc_0;
  DEFER(java_proc_0.Kill());
  ASSERT_OK(java_proc_0.Start({java_app_path}));
  constexpr uint64_t start_time_ns = 0;
  const uint32_t child_pid_0 = java_proc_0.child_pid();
  const struct upid_t child_upid_0 = {{child_pid_0}, start_time_ns};

  symbolizer->IterationPreTick();
  symbolizer->GetSymbolizerFn(child_upid_0);
  std::this_thread::sleep_for(std::chrono::milliseconds{500});

  ASSERT_TRUE(symbolizer->Uncacheable(child_upid_0)) << "Should have found symbol file by now.";
  const auto artifacts_path_0 = java::AgentArtifactsPath(child_upid_0);
  EXPECT_TRUE(fs::Exists(artifacts_path_0));

  // Disable java symbolization.
  FLAGS_stirling_profiler_java_symbols = false;

  // Disabling Java symbolization should not affect Java processes that we previously symbolized.
  EXPECT_TRUE(symbolizer->Uncacheable(child_upid_0)) << "Should have found symbol file by now.";
  EXPECT_TRUE(fs::Exists(artifacts_path_0));

  // Start a new Java sub-process.
  // We expect that it will not have a JVMTI symbolization agent injected.
  SubProcess java_proc_1;
  DEFER(java_proc_1.Kill());
  ASSERT_OK(java_proc_1.Start({java_app_path}));
  const uint32_t child_pid_1 = java_proc_1.child_pid();
  const struct upid_t child_upid_1 = {{child_pid_1}, start_time_ns};

  symbolizer->GetSymbolizerFn(child_upid_1);
  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  // Expect the symbols remain cacheable (non-Java).
  // Expect to *not* find the Java symbolization artifacts.
  EXPECT_FALSE(symbolizer->Uncacheable(child_upid_1));
  const auto artifacts_path_1 = java::AgentArtifactsPath(child_upid_1);
  EXPECT_FALSE(fs::Exists(artifacts_path_1));

  // Expect that JVMTI agent injection tracking includes sub-proc-0 but not sub-proc-1.
  EXPECT_TRUE(JavaProfilingProcTracker::GetSingleton()->upids().contains(child_upid_0));
  EXPECT_FALSE(JavaProfilingProcTracker::GetSingleton()->upids().contains(child_upid_1));
}

// Java symbolizer does not attach if not enough space is available.
// The tmpfs used for a /tmp volume mount (below, where the docker volume is created)
// is sized too small for our agent libs.
TEST_F(BCCSymbolizerTest, JavaNotEnoughSpaceAvailable) {
  // Populate locally scoped flags values that setup the test environment.
  // Agent libs & px_jattach need to be found inside of the bazel env., populated via helper fns.
  // We also ensure that Java symbolization is enabled.
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);

  // Create a Java symbolizer.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  // Ensure necessary test collateral can be found using fs::Exists().
  // We will need "px_jattach" and a containerized Java test app., from "FLAGS_java_image_name".
  ASSERT_TRUE(fs::Exists(FLAGS_stirling_profiler_px_jattach_path))
      << FLAGS_stirling_profiler_px_jattach_path;
  ASSERT_TRUE(fs::Exists(FLAGS_java_image_name)) << FLAGS_java_image_name;

  // Setup commands to create and remove a docker volume.
  // It will be named and sized according to "volume_name" and "tmpfs_size_arg".
  const pid_t pid = getpid();
  const std::string volume_name = absl::Substitute("px-tmp-jsyms-not-enough-space-test-$0", pid);
  const std::string tmpfs_size_arg = "size=500K";
  const std::string create_volume = absl::Substitute(
      "docker volume create --driver local --opt type=tmpfs --opt device=tmpfs --opt o=$0 $1",
      tmpfs_size_arg, volume_name);
  const std::string remove_volume = absl::Substitute("docker volume rm $0", volume_name);

  // Create the docker volume. Defer removal of the same.
  ASSERT_EQ(0, ::system(create_volume.c_str()));
  DEFER(ASSERT_EQ(0, ::system(remove_volume.c_str())););

  // Instantiate a ContainerRunner for our containerized test app.
  static constexpr std::string_view kReadyMsg = "";
  const std::filesystem::path image_tar_path(FLAGS_java_image_name);
  static constexpr std::string_view container_name_pfx = "java";
  ContainerRunner sub_process(image_tar_path, container_name_pfx, kReadyMsg);

  // Start the container/sub-proc. Use "-v" arg. to mount our tmpfs into /tmp in that container.
  const std::string tmpfs_mount_opt = absl::Substitute("$0:/tmp", volume_name);
  static constexpr auto timeout = std::chrono::seconds{600};
  static constexpr bool kUseHostPidNamespace = false;
  const std::vector<std::string> options = {"-v", tmpfs_mount_opt};
  const std::vector<std::string> args;
  sub_process.Run(timeout, options, args, kUseHostPidNamespace);

  // Construct a upid (with a placeholder start time) for the sub-proc.
  constexpr uint64_t start_time_ns = 0;
  const uint32_t child_pid = sub_process.process_pid();
  const struct upid_t child_upid = {{child_pid}, start_time_ns};

  // Force an iteration "pre tick" event in the symbolizer. This resets the budget for the number
  // of attach events allowed "per iteration" based on FLAGS_number_attach_attempts_per_iteration.
  // A freshly minted symbolizer starts with its budget set to zero. We do this here so that
  // the symbolizer can attempt (as many times as needed) a JVMTI symbolization agent attach.
  PX_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);
  symbolizer->IterationPreTick();

  // Requesting the symbolization function kicks off the attach process, i.e. because the symbolizer
  // will not have a cached symbolization function for this upid.
  symbolizer->GetSymbolizerFn(child_upid);

  // Give the attach process some time (more than enough time) to complete.
  std::this_thread::sleep_for(std::chrono::milliseconds{500});

  // The symbols themselves will remain as "cacheable" i.e. handled by a normal non-Java symbolizer,
  // because no JVMTI agents were attached because no space was available. Java symbols are
  // considered uncacheable becasue the JVM is free to delete them
  // and recompile them to a different location.
  // Succinctly, this test expects JVMTI attach to abort because tmpfs was too small.
  ASSERT_FALSE(symbolizer->Uncacheable(child_upid)) << "Symbolizer should fail to attach.";
}

// This is the same test as "JavaNotEnoughSpaceAvailable" but setup with enough tmpfs capacity
// for the Java agent libs. Running this test increases confidence that the reason
// "JavaNotEnoughSpaceAvailable" passes (if it passes) is because the Java symbolizer did
// not come up specifically because of the small tmpfs and not for some other unrelated reason.
TEST_F(BCCSymbolizerTest, JavaEnoughSpaceAvailable) {
  // Populate locally scoped flags values that setup the test environment.
  // Agent libs & px_jattach need to be found inside of the bazel env., populated via helper fns.
  // We also ensure that Java symbolization is enabled.
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);

  // Create a Java symbolizer.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  // Ensure necessary test collateral can be found using fs::Exists().
  // We will need "px_jattach" and a containerized Java test app., from "FLAGS_java_image_name".
  ASSERT_TRUE(fs::Exists(FLAGS_stirling_profiler_px_jattach_path))
      << FLAGS_stirling_profiler_px_jattach_path;
  ASSERT_TRUE(fs::Exists(FLAGS_java_image_name)) << FLAGS_java_image_name;

  // Setup commands to create and remove a docker volume.
  // It will be named and sized according to "volume_name" and "tmpfs_size_arg".
  const pid_t pid = getpid();
  const std::string volume_name = absl::Substitute("px-tmp-jsyms-enough-space-test-$0", pid);
  const std::string tmpfs_size_arg = "size=20M";
  const std::string create_volume = absl::Substitute(
      "docker volume create --driver local --opt type=tmpfs --opt device=tmpfs --opt o=$0 $1",
      tmpfs_size_arg, volume_name);
  const std::string remove_volume = absl::Substitute("docker volume rm $0", volume_name);

  // Create the docker volume. Defer removal of the same.
  ASSERT_EQ(0, ::system(create_volume.c_str()));
  DEFER(ASSERT_EQ(0, ::system(remove_volume.c_str())););

  // Instantiate a ContainerRunner for our containerized test app.
  static constexpr std::string_view kReadyMsg = "";
  const std::filesystem::path image_tar_path(FLAGS_java_image_name);
  static constexpr std::string_view container_name_pfx = "java";
  ContainerRunner sub_process(image_tar_path, container_name_pfx, kReadyMsg);

  // Start the container/sub-proc. Use "-v" arg. to mount our tmpfs into /tmp in that container.
  const std::string tmpfs_mount_opt = absl::Substitute("$0:/tmp", volume_name);
  static constexpr auto timeout = std::chrono::seconds{600};
  static constexpr bool kUseHostPidNamespace = false;
  const std::vector<std::string> options = {"-v", tmpfs_mount_opt};
  const std::vector<std::string> args;
  sub_process.Run(timeout, options, args, kUseHostPidNamespace);

  // Construct a upid (with a placeholder start time) for the sub-proc.
  constexpr uint64_t start_time_ns = 0;
  const uint32_t child_pid = sub_process.process_pid();
  const struct upid_t child_upid = {{child_pid}, start_time_ns};

  // Force an iteration "pre tick" event in the symbolizer. This resets the budget for the number
  // of attach events allowed "per iteration" based on FLAGS_number_attach_attempts_per_iteration.
  // A freshly minted symbolizer starts with its budget set to zero. We do this here so that
  // the symbolizer can attempt (as many times as needed) a JVMTI symbolization agent attach.
  PX_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);
  symbolizer->IterationPreTick();

  // Requesting the symbolization function kicks off the attach process, i.e. because the symbolizer
  // will not have a cached symbolization function for this upid.
  symbolizer->GetSymbolizerFn(child_upid);

  // Give the attach process some time (more than enough time) to complete.
  std::this_thread::sleep_for(std::chrono::milliseconds{500});

  // Java symbols are considered uncacheable becasue the JVM is free to delete them
  // and recompile them to a different location. We can infer successful JVMTI symbolization
  // agent attach by the symbolizer reporting that the symbols are indeed uncacheable.
  // Succinctly, this test expects JVMTI attach success because tmpfs had enough space.
  ASSERT_TRUE(symbolizer->Uncacheable(child_upid)) << "Symbolizer did not attach.";
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
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_agent_libs, GetAgentLibsFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_px_jattach_path, GetPxJattachFlagValueForTesting());
  PX_SET_FOR_SCOPE(FLAGS_stirling_profiler_java_symbols, true);
  PX_SET_FOR_SCOPE(FLAGS_number_attach_attempts_per_iteration, 10000);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Symbolizer> symbolizer,
                       JavaSymbolizer::Create(std::move(symbolizer_)));

  SubProcess fake_java_proc;
  DEFER(fake_java_proc.Kill());
  const std::filesystem::path fake_java_bin_path =
      BazelRunfilePath("src/stirling/source_connectors/perf_profiler/testing/java/java");
  ASSERT_TRUE(fs::Exists(fake_java_bin_path));
  ASSERT_OK(fake_java_proc.Start({fake_java_bin_path}));
  const uint32_t child_pid = fake_java_proc.child_pid();

  const system::ProcParser parser;
  ASSERT_OK_AND_ASSIGN(const uint64_t start_time_ns, parser.GetPIDStartTimeTicks(child_pid));

  const struct upid_t child_upid = {{child_pid}, start_time_ns};
  symbolizer->IterationPreTick();
  symbolizer->GetSymbolizerFn(child_upid);
  EXPECT_TRUE(JavaProfilingProcTracker::GetSingleton()->upids().contains(child_upid));
}

}  // namespace stirling
}  // namespace px
