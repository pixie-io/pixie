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

#include <algorithm>
#include <atomic>
#include <utility>

#include <sys/sysinfo.h>

#include <absl/strings/match.h>
#include <absl/strings/substitute.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/core/unit_connector.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/java_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/testing/testing.h"
#include "src/stirling/testing/common.h"

DEFINE_uint32(test_run_time, 10, "Number of seconds to run the test.");
DEFINE_string(test_java_image_names, JDK_IMAGE_NAMES,
              "Java docker images to use as Java test cases.");
DECLARE_bool(stirling_profiler_java_symbols);
DECLARE_string(stirling_profiler_java_agent_libs);
DECLARE_uint32(stirling_profiler_table_update_period_seconds);
DECLARE_uint32(stirling_profiler_stack_trace_sample_period_ms);

namespace px {
namespace stirling {

using ::px::stirling::profiler::testing::GetAgentLibsFlagValueForTesting;
using ::px::stirling::profiler::testing::GetPxJattachFlagValueForTesting;
using ::px::stirling::testing::FindRecordIdxMatchesPIDs;
using ::px::testing::BazelRunfilePath;
using ::px::testing::PathExists;
using ::testing::Each;
using ::testing::Not;
using ::testing::SizeIs;

namespace {
// BazelXXTestAppPath() are helpers to build & return the bazelified path to
// one of the toy apps used by perf profiler testing.
std::filesystem::path BazelCCTestAppPath(const std::string_view app_name) {
  const std::filesystem::path kToyAppsPath =
      "src/stirling/source_connectors/perf_profiler/testing/cc";
  const std::filesystem::path app_path = kToyAppsPath / app_name / app_name;
  const std::filesystem::path bazel_app_path = BazelRunfilePath(app_path);
  return bazel_app_path;
}

std::filesystem::path BazelGoTestAppPath(const std::string_view app_name) {
  char const* const go_path_pattern = "src/stirling/source_connectors/perf_profiler/testing/go/$0_";
  const std::filesystem::path sub_path = absl::Substitute(go_path_pattern, app_name);
  const std::filesystem::path app_path = sub_path / app_name;
  const std::filesystem::path bazel_app_path = BazelRunfilePath(app_path);
  return bazel_app_path;
}

std::filesystem::path BazelJavaTestAppPath(const std::string_view app_name) {
  const std::filesystem::path kToyAppsPath =
      "src/stirling/source_connectors/perf_profiler/testing/java";
  const std::filesystem::path app_path = kToyAppsPath / app_name;
  const std::filesystem::path bazel_app_path = BazelRunfilePath(app_path);
  return bazel_app_path;
}

absl::flat_hash_map<std::string, uint64_t> KeepNLeafSyms(
    const uint64_t n, const absl::flat_hash_map<std::string, uint64_t>& stack_trace_histo) {
  absl::flat_hash_map<std::string, uint64_t> leaf_histo;

  for (const auto& [stack_trace_str, count] : stack_trace_histo) {
    const std::vector<std::string_view> symbols = absl::StrSplit(stack_trace_str, ";");

    const auto begin_iter = n > symbols.size() ? symbols.begin() : symbols.end() - n;
    const auto end_iter = symbols.end();

    const auto leaf_syms = absl::StrJoin(begin_iter, end_iter, ";");
    leaf_histo[leaf_syms] += count;
  }
  return leaf_histo;
}

}  // namespace

class PerfProfilerTestSubProcesses {
 public:
  virtual void StartAll() = 0;
  virtual void KillAll() = 0;
  const std::vector<int>& pids() const { return pids_; }
  const std::vector<struct upid_t>& struct_upids() const { return struct_upids_; }
  const absl::flat_hash_set<md::UPID>& upids() const { return upids_; }
  static constexpr size_t kNumSubProcs = 4;
  virtual ~PerfProfilerTestSubProcesses() = default;

 protected:
  std::vector<int> pids_;
  std::vector<struct upid_t> struct_upids_;
  absl::flat_hash_set<md::UPID> upids_;
};

class CPUPinnedSubProcesses final : public PerfProfilerTestSubProcesses {
 public:
  CPUPinnedSubProcesses(const std::string& binary_path) : binary_path_(binary_path) {}

  ~CPUPinnedSubProcesses() { KillAll(); }

  void StartAll() override {
    ASSERT_TRUE(fs::Exists(binary_path_));
    ASSERT_TRUE(fs::Exists(kTasksetBinPath));
    const auto processor_count = std::thread::hardware_concurrency();
    if (processor_count > 0) {
      ASSERT_GE(processor_count, kNumSubProcs);
    }

    const system::ProcParser proc_parser;

    for (size_t i = 0; i < kNumSubProcs; ++i) {
      auto sub_process = std::make_unique<SubProcess>();

      // Run the sub-process & pin it to a CPU.
      std::string mask = absl::StrFormat("%#x", 1 << i);
      ASSERT_OK(sub_process->Start({std::string(kTasksetBinPath), mask, binary_path_}));

      // Grab the PID and generate a UPID.
      const int pid = sub_process->child_pid();
      ASSERT_OK_AND_ASSIGN(const uint64_t ts, proc_parser.GetPIDStartTimeTicks(pid));
      pids_.push_back(pid);
      struct_upids_.push_back({{static_cast<uint32_t>(pid)}, ts});
      upids_.emplace(0, pid, ts);
      sub_processes_.emplace_back(std::move(sub_process));
    }
  }

  void KillAll() override {
    for (auto& sub_process : sub_processes_) {
      sub_process->Kill();
    }

    // This will release all the managed pointers and run the dtors.
    sub_processes_.clear();
  }

 private:
  static constexpr std::string_view kTasksetBinPath = "/bin/taskset";
  std::vector<std::unique_ptr<SubProcess>> sub_processes_;
  const std::string binary_path_;
};

class ContainerSubProcesses final : public PerfProfilerTestSubProcesses {
 public:
  ContainerSubProcesses(const std::filesystem::path image_tar_path,
                        const std::string_view container_name_pfx) {
    for (size_t i = 0; i < kNumSubProcs; ++i) {
      sub_processes_.push_back(
          std::make_unique<ContainerRunner>(image_tar_path, container_name_pfx, kReadyMsg));
    }
  }

  ~ContainerSubProcesses() { KillAll(); }

  void StartAll() override {
    const system::ProcParser proc_parser;
    const auto timeout = std::chrono::seconds{2 * FLAGS_test_run_time};
    const std::vector<std::string> opts = {"--mount", "type=tmpfs,tmpfs-size=64M,destination=/tmp"};
    const std::vector<std::string> args;
    static constexpr bool kUseHostPidNamespace = false;

    for (size_t i = 0; i < kNumSubProcs; ++i) {
      sub_processes_[i]->Run(timeout, opts, args, kUseHostPidNamespace);

      // Grab the PID and generate a UPID.
      const int pid = sub_processes_[i]->process_pid();
      ASSERT_OK_AND_ASSIGN(const uint64_t ts, proc_parser.GetPIDStartTimeTicks(pid));
      pids_.push_back(pid);
      struct_upids_.push_back({{static_cast<uint32_t>(pid)}, ts});
      upids_.emplace(0, pid, ts);
    }
  }

  void KillAll() override {
    // This kills the containerized processes (by running the dtors).
    sub_processes_.clear();
  }

 private:
  static constexpr std::string_view kReadyMsg = "";
  std::vector<std::unique_ptr<ContainerRunner>> sub_processes_;
};

template <bool FastTest>
class PerfProfileBPFTest : public ::testing::TestWithParam<std::filesystem::path> {
 protected:
  void SetUp() override {
    FLAGS_stirling_profiler_java_symbols = true;
    FLAGS_number_attach_attempts_per_iteration = kNumSubProcs;

    if constexpr (FastTest) {
      // Increase frequency of TransferData().
      FLAGS_stirling_profiler_table_update_period_seconds = 1;

      // Increase frequency at which we sample stack traces in underlying eBPF probe.
      FLAGS_stirling_profiler_stack_trace_sample_period_ms = 7;

      // Shorten the test run time.
      test_run_time_ = std::chrono::seconds{FLAGS_test_run_time};
    } else {
      // Accept perf profiler defaults for freq. of TransferData() and stack trace sampling.
      // Set test run time long enough to capture one iter. of TransferData().
      test_run_time_ = std::chrono::seconds{FLAGS_stirling_profiler_table_update_period_seconds};
    }
  }

  void PopulateObservedStackTraces(const std::vector<size_t>& target_row_idxs) {
    auto stack_traces_column = columns_[kStackTraceStackTraceStrIdx];
    auto counts_column = columns_[kStackTraceCountIdx];

    for (const auto row_idx : target_row_idxs) {
      // Build the histogram of observed stack traces here.
      const std::string stack_trace_str = stack_traces_column->Get<types::StringValue>(row_idx);
      const int64_t count = counts_column->Get<types::Int64Value>(row_idx).val;
      histo_[stack_trace_str] += count;
    }

    // TODO(jps): bring in a 3rd party library for colorization. e.g., one of the following:
    // https://github.com/agauniyal/rang
    // https://github.com/ikalnytskyi/termcolor
    auto makecolor = [](const auto n) { return absl::StrFormat("\x1b[38;5;$%dm", n); };
    auto reset = []() { return "\x1b[0m"; };
    VLOG(1) << std::endl;
    for (const auto& [key, val] : histo_) {
      VLOG(1) << makecolor(220) << absl::StrFormat("%5d: ", val) << key << reset() << std::endl;
    }
    VLOG(1) << std::endl;
  }

  void PopulateCumulativeSum(const std::vector<size_t>& target_row_idxs) {
    cumulative_sum_ = 0;
    auto counts_column = columns_[kStackTraceCountIdx];

    for (const auto row_idx : target_row_idxs) {
      const int64_t count = counts_column->Get<types::Int64Value>(row_idx).val;
      cumulative_sum_ += count;
      ASSERT_GT(count, 0);
    }
  }

  void CheckExpectedSampleRate(const ssize_t num_subprocesses) {
    const uint64_t table_period_ms = source_.RawPtr()->SamplingPeriod().count();
    const uint64_t bpf_period_ms = source_.RawPtr()->StackTraceSamplingPeriod().count();
    const double expected_rate = 1000.0 / static_cast<double>(bpf_period_ms);
    const double expected_num_samples = num_subprocesses * t_elapsed_.count() * expected_rate;
    const uint64_t expected_num_sample_lower = uint64_t(0.9 * expected_num_samples);
    const uint64_t expected_num_sample_upper = uint64_t(1.1 * expected_num_samples);
    const double observedNumSamples = static_cast<double>(cumulative_sum_);
    const double observed_rate = observedNumSamples / t_elapsed_.count() / num_subprocesses;

    LOG(INFO) << absl::StrFormat("Table sampling period: %d [ms].", table_period_ms);
    LOG(INFO) << absl::StrFormat("BPF sampling period: %d [ms].", bpf_period_ms);
    LOG(INFO) << absl::StrFormat("Number of processes: %d.", num_subprocesses);
    LOG(INFO) << absl::StrFormat("expected num samples: %d.", uint64_t(expected_num_samples));
    LOG(INFO) << absl::StrFormat("total samples: %d.", cumulative_sum_);
    LOG(INFO) << absl::StrFormat("elapsed time: %.1f [sec].", t_elapsed_.count());
    LOG(INFO) << absl::StrFormat("expected sample rate: %.2f [Hz].", expected_rate);
    LOG(INFO) << absl::StrFormat("observed sample rate: %.2f [Hz].", observed_rate);

    // We expect to see a certain number of samples, but in practice
    // see fewer (maybe the CPU and Linux scheduler have other things to do!).
    // For this test, use an upper & lower bound with 10% allowed error band.
    const std::string err_msg = absl::StrFormat(
        "num sub-processes: %d, time: %.2f [sec.], rate: %.2f [Hz], observed_rate: %.2f [Hz]",
        num_subprocesses, t_elapsed_.count(), expected_rate, observed_rate);
    EXPECT_GT(cumulative_sum_, expected_num_sample_lower) << err_msg;
    EXPECT_LT(cumulative_sum_, expected_num_sample_upper) << err_msg;
  }

  void CheckExpectedProfile(const absl::flat_hash_map<std::string, uint64_t>& counts,
                            const std::string_view key1x, const std::string_view key2x) {
    char const* const missing_key_msg = "Could not find required symbol or stack trace: $0.";
    ASSERT_TRUE(counts.find(key1x) != counts.end()) << absl::Substitute(missing_key_msg, key1x);
    ASSERT_TRUE(counts.find(key2x) != counts.end()) << absl::Substitute(missing_key_msg, key2x);

    const double key1x_count = counts.at(key1x);
    const double key2x_count = counts.at(key2x);
    const double ratio = key2x_count / key1x_count;

    // We expect the ratio of key2x:key1x to be approx. 2:1.
    LOG(INFO) << absl::StrFormat("key2x: %s.", key2x);
    LOG(INFO) << absl::StrFormat("key1x: %s.", key1x);
    LOG(INFO) << absl::StrFormat("key2x count: %d.", static_cast<uint64_t>(key2x_count));
    LOG(INFO) << absl::StrFormat("key1x count: %d.", static_cast<uint64_t>(key1x_count));
    LOG(INFO) << absl::StrFormat("ratio: %.2fx.", ratio);

    EXPECT_GT(ratio, 2.0 - kRatioMargin);
    EXPECT_LT(ratio, 2.0 + kRatioMargin);

    EXPECT_EQ(source_.RawPtr()->stats().Get(PerfProfileConnector::StatKey::kLossHistoEvent), 0);
  }

  void ConsumeRecords() {
    ASSERT_OK_AND_ASSIGN(columns_, source_.ConsumeRecords(kProfilerTableNum));
    auto target_row_idxs =
        FindRecordIdxMatchesPIDs(columns_, kStackTraceUPIDIdx, sub_processes_->pids());

    PopulateCumulativeSum(target_row_idxs);
    PopulateObservedStackTraces(target_row_idxs);
  }

  Status RunTest() {
    PX_RETURN_IF_ERROR(source_.Init(sub_processes_->upids()));
    PX_RETURN_IF_ERROR(source_.Start());

    const auto start_time = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(test_run_time_);

    PX_RETURN_IF_ERROR(source_.Stop());

    const auto end_time = std::chrono::steady_clock::now();
    t_elapsed_ = std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time);

    return Status::OK();
  }

  std::unique_ptr<PerfProfilerTestSubProcesses> sub_processes_;

  uint64_t cumulative_sum_ = 0;
  absl::flat_hash_map<std::string, uint64_t> histo_;

  types::ColumnWrapperRecordBatch columns_;

  // To reduce variance in results, we add more run-time or add sub-processes:
  static constexpr uint64_t kNumSubProcs = 4;
  static constexpr double kRatioMargin = 0.5;

  static constexpr uint32_t kProfilerTableNum = 0;

  // Target test run time. Used by RunTest().
  std::chrono::seconds test_run_time_;

  // Elapsed test run time. Set by RunTest().
  std::chrono::duration<double> t_elapsed_;

  UnitConnector<PerfProfileConnector> source_;
};

class FastPerfProfileBPFTest : public PerfProfileBPFTest</* FastTest */ true> {};

class SlowPerfProfileBPFTest : public PerfProfileBPFTest</* FastTest */ false> {};

TEST_F(SlowPerfProfileBPFTest, PerfProfilerGoTest) {
  const std::filesystem::path bazel_app_path = BazelGoTestAppPath("profiler_test_app_sqrt_go");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // clang-format off
  // The target app is written such that key2x uses twice the CPU time as key1x.
  constexpr std::string_view key2x = "runtime.goexit.abi0;runtime.main;main.main;main.sqrtOf1e39;main.sqrt";  // NOLINT(whitespace/line_length)
  constexpr std::string_view key1x = "runtime.goexit.abi0;runtime.main;main.main;main.sqrtOf1e18;main.sqrt";  // NOLINT(whitespace/line_length)
  // clang-format on

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  // Bring up (and later stop) the perf profile source connector, setup test params,
  // and allow target apps to run for the allotted time.
  ASSERT_OK(RunTest());

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(CheckExpectedSampleRate(kNumSubProcs));
  ASSERT_NO_FATAL_FAILURE(CheckExpectedProfile(histo_, key1x, key2x));
}

TEST_F(FastPerfProfileBPFTest, PerfProfilerCppTest) {
  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // The target app is written such that key2x uses twice the CPU time as key1x.
  constexpr std::string_view key2x = "main;fib52();fib(unsigned long)";
  constexpr std::string_view key1x = "main;fib27();fib(unsigned long)";

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  // Bring up (and later stop) the perf profile source connector, setup test params,
  // and allow target apps to run for the allotted time.
  ASSERT_OK(RunTest());

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  const auto leaf_histo = KeepNLeafSyms(3, histo_);
  ASSERT_NO_FATAL_FAILURE(CheckExpectedSampleRate(kNumSubProcs));
  ASSERT_NO_FATAL_FAILURE(CheckExpectedProfile(leaf_histo, key1x, key2x));
}

TEST_F(FastPerfProfileBPFTest, GraalVM_AOT_Test) {
  const std::string app_path = "ProfilerTest";
  const std::filesystem::path bazel_app_path = BazelJavaTestAppPath(app_path);
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // The target app is written such that key2x uses twice the CPU time as key1x.
  // For Java, we will match only the leaf symbol because we cannot predict the full stack trace.
  constexpr std::string_view key2x = "ProfilerTest_leaf2x_2971a14bad627821bd5c46dbdf969a8ab42430f5";
  constexpr std::string_view key1x = "ProfilerTest_leaf1x_41af06c0834431228b8c265075a583c347b33636";

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  // Bring up (and later stop) the perf profile source connector, setup test params,
  // and allow target apps to run for the allotted time.
  ASSERT_OK(RunTest());

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  const auto leaf_histo = KeepNLeafSyms(1, histo_);
  ASSERT_NO_FATAL_FAILURE(CheckExpectedSampleRate(kNumSubProcs));
  ASSERT_NO_FATAL_FAILURE(CheckExpectedProfile(leaf_histo, key1x, key2x));
}

TEST_P(FastPerfProfileBPFTest, PerfProfilerJavaTest) {
  constexpr std::string_view kContainerNamePfx = "java";
  const std::filesystem::path image_tar_path = GetParam();
  ASSERT_TRUE(fs::Exists(image_tar_path)) << absl::StrFormat("Missing: %s.", image_tar_path);

  // The target app is written such that key2x uses twice the CPU time as key1x.
  // For Java, we will match only the leaf symbol because we cannot predict the full stack trace.
  constexpr std::string_view key2x = "[j] long ProfilerTest::leaf2x()";
  constexpr std::string_view key1x = "[j] long ProfilerTest::leaf1x()";

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<ContainerSubProcesses>(image_tar_path, kContainerNamePfx);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  // Bring up (and later stop) the perf profile source connector, setup test params,
  // and allow target apps to run for the allotted time.
  ASSERT_OK(RunTest());

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  const auto leaf_histo = KeepNLeafSyms(1, histo_);
  ASSERT_NO_FATAL_FAILURE(CheckExpectedProfile(leaf_histo, key1x, key2x));
  ASSERT_NO_FATAL_FAILURE(CheckExpectedSampleRate(kNumSubProcs));
}

using ::px::stirling::testing::Timeout;

TEST_F(FastPerfProfileBPFTest, PerfProfilerJavaArtifactsCleanupTest) {
  const std::filesystem::path bazel_app_path = BazelJavaTestAppPath("profiler_test");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  for (const auto pid : sub_processes_->pids()) {
    LOG(WARNING) << "Started sub-process: " << pid << ".";
  }

  ASSERT_OK(source_.Init(sub_processes_->upids()));
  ASSERT_OK(source_.Start());

  std::set<std::filesystem::path> artifacts_paths;

  {
    Timeout timer;
    while (!timer.TimedOut() && artifacts_paths.size() < kNumSubProcs) {
      for (const auto& upid : sub_processes_->struct_upids()) {
        const auto artifacts_path = java::StirlingArtifactsPath(upid);
        if (fs::Exists(artifacts_path) && artifacts_paths.count(artifacts_path) == 0) {
          artifacts_paths.insert(artifacts_path);
        }
      }
    }
  }
  EXPECT_THAT(artifacts_paths, SizeIs(kNumSubProcs));

  // Wait to see a Java symbol from each sub-process, i.e. so that we know the symbolization
  // artifact was created and populated.
  std::set<int> pids_with_java_symbols;
  {
    Timeout timer;
    while (!timer.TimedOut() && pids_with_java_symbols.size() < kNumSubProcs) {
      ASSERT_OK(source_.Flush());
      auto s = source_.ConsumeRecords(kProfilerTableNum);
      if (s.ok()) {
        columns_ = s.ConsumeValueOrDie();
        auto stack_traces_column = columns_[kStackTraceStackTraceStrIdx];
        for (const auto pid : sub_processes_->pids()) {
          if (pids_with_java_symbols.find(pid) != pids_with_java_symbols.end()) {
            // Already found, ok to continue.
            continue;
          }

          // This returns a list of each symbol in all the stack traces for a given pid.
          auto AllSymbolsForPid = [&](const int pid) {
            std::vector<std::string> stack_traces;

            const auto rows_idxs = FindRecordIdxMatchesPIDs(columns_, kStackTraceUPIDIdx, {pid});
            for (const auto row_idx : rows_idxs) {
              const auto stack_trace = stack_traces_column->Get<types::StringValue>(row_idx);
              stack_traces.push_back(stack_trace);
            }
            const std::string all_symbols = absl::StrJoin(stack_traces, ";");
            const std::vector<std::string_view> symbols = absl::StrSplit(all_symbols, ";");
            return symbols;
          };

          // Iterate over all the symbols for this pid to look for a Java symbol.
          for (const auto symbol : AllSymbolsForPid(pid)) {
            if (absl::StartsWith(symbol, "[j]")) {
              // The Java symbolizer found a Java symbol for this process.
              pids_with_java_symbols.insert(pid);
              break;
            }
          }
        }
      }
    }
  }

  // We expect that we found a Java symbol for each sub-process.
  EXPECT_THAT(pids_with_java_symbols, SizeIs(kNumSubProcs));

  // Kill the subprocs.
  sub_processes_->KillAll();

  // Need to pass in an invalid upid set to prevent picking up the default context that
  // accepts all pids.
  const md::UPID invalid_upid = {0, 0, 0};
  const absl::flat_hash_set<md::UPID> invalid_upid_set = {invalid_upid};
  ASSERT_OK(source_.ResetConnectorContext(invalid_upid_set));

  // Now that the sub-processes are gone, iterate over the previously found artifact paths.
  // If the artifact path can no longer be found, we remove it from the set.
  {
    Timeout timer;
    std::set<std::filesystem::path> removed_artifacts_paths;
    while (!timer.TimedOut() && artifacts_paths.size() > 0) {
      for (const auto& artifacts_path : artifacts_paths) {
        if (!fs::Exists(artifacts_path)) {
          artifacts_paths.erase(artifacts_path);
          break;
        }
      }
    }
  }

  // This is the main event:
  // we expect that the profiler cleans up each artifact.
  EXPECT_THAT(artifacts_paths, SizeIs(0));

  ASSERT_OK(source_.Stop());
}

TEST_F(FastPerfProfileBPFTest, TestOutOfContext) {
  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  // The "out of context" test needs to handle init., param. setup, and start each individually,
  //  so that it can pass in an empty set of upids rather than the subprocess upids.
  const md::UPID invalid_upid = {0, 0, 0};
  const absl::flat_hash_set<md::UPID> invalid_upid_set = {invalid_upid};
  ASSERT_OK(source_.Init(invalid_upid_set));
  ASSERT_OK(source_.Start());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  std::this_thread::sleep_for(test_run_time_);

  ASSERT_OK(source_.Stop());

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
}

std::vector<std::filesystem::path> GetJavaImagePaths() {
  const std::vector<std::string_view> image_names =
      absl::StrSplit(FLAGS_test_java_image_names, ",");
  std::vector<std::filesystem::path> image_paths;
  for (const auto image_name : image_names) {
    image_paths.push_back(BazelJavaTestAppPath(std::string(image_name) + ".tar"));
  }
  return image_paths;
}

INSTANTIATE_TEST_SUITE_P(PerfProfileJavaTests, FastPerfProfileBPFTest,
                         ::testing::ValuesIn(GetJavaImagePaths()));

}  // namespace stirling
}  // namespace px
