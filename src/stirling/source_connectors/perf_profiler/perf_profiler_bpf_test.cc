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

#include <absl/strings/substitute.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/stirling/core/connector_context.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/java_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/testing/testing.h"
#include "src/stirling/testing/common.h"

DEFINE_uint32(test_run_time, 30, "Number of seconds to run the test.");
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
using ::testing::Gt;
using ::testing::Not;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

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
    leaf_histo[leaf_syms] = count;
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
    const system::ProcParser proc_parser;

    for (size_t i = 0; i < kNumSubProcs; ++i) {
      sub_processes_.push_back(std::make_unique<SubProcess>());

      // Run the sub-process & pin it to a CPU.
      const std::string kTasksetBinPath = "/usr/bin/taskset";
      ASSERT_OK(sub_processes_[i]->Start({kTasksetBinPath, "-c", std::to_string(i), binary_path_}));

      // Grab the PID and generate a UPID.
      const int pid = sub_processes_[i]->child_pid();
      ASSERT_OK_AND_ASSIGN(const uint64_t ts, proc_parser.GetPIDStartTimeTicks(pid));
      pids_.push_back(pid);
      struct_upids_.push_back({{static_cast<uint32_t>(pid)}, ts});
      upids_.emplace(0, pid, ts);
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
  static constexpr std::string_view kTasksetBinPath = "/usr/bin/taskset";
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
    const std::vector<std::string> options;
    const std::vector<std::string> args;
    static constexpr bool kUseHostPidNamespace = false;

    for (size_t i = 0; i < kNumSubProcs; ++i) {
      sub_processes_[i]->Run(timeout, options, args, kUseHostPidNamespace);

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

class PerfProfileBPFTest : public ::testing::TestWithParam<std::filesystem::path> {
 public:
  PerfProfileBPFTest()
      : test_run_time_(FLAGS_test_run_time), data_table_(/*id*/ 0, kStackTraceTable) {}

 protected:
  void SetUp() override {
    FLAGS_stirling_profiler_java_symbols = true;
    FLAGS_number_attach_attempts_per_iteration = kNumSubProcs;
    FLAGS_stirling_profiler_table_update_period_seconds = 5;
    FLAGS_stirling_profiler_stack_trace_sample_period_ms = 7;

    source_ = PerfProfileConnector::Create("perf_profile_connector");
    ASSERT_OK(source_->Init());
    source_->set_data_tables({&data_table_});

    // Immediately start the transfer data thread to continue draining perf buffers,
    // i.e. to prevent dropping perf buffer entries.
    StartTransferDataThread();
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  void PopulateObservedStackTraces(const std::vector<size_t>& target_row_idxs) {
    // Just check that the test author populated the necessary,
    // and did not corrupt the cumulative sum already.
    ASSERT_TRUE(column_ptrs_populated_);

    for (const auto row_idx : target_row_idxs) {
      // Build the histogram of observed stack traces here.
      const std::string stack_trace_str = stack_traces_column_->Get<types::StringValue>(row_idx);
      const int64_t count = counts_column_->Get<types::Int64Value>(row_idx).val;
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
    ASSERT_TRUE(column_ptrs_populated_);
    ASSERT_EQ(cumulative_sum_, 0);

    for (const auto row_idx : target_row_idxs) {
      const int64_t count = counts_column_->Get<types::Int64Value>(row_idx).val;
      cumulative_sum_ += count;
      ASSERT_GT(count, 0);
    }
  }

  void CheckExpectedCounts(const absl::flat_hash_map<std::string, uint64_t>& counts,
                           const ssize_t num_subprocesses,
                           const std::chrono::duration<double> t_elapsed,
                           const std::string_view key1x, const std::string_view key2x) {
    const uint64_t table_period_ms = source_->SamplingPeriod().count();
    const uint64_t bpf_period_ms = source_->StackTraceSamplingPeriod().count();
    const double expected_rate = 1000.0 / static_cast<double>(bpf_period_ms);
    const double expected_num_samples = num_subprocesses * t_elapsed.count() * expected_rate;
    const uint64_t expected_num_sample_lower = uint64_t(0.9 * expected_num_samples);
    const uint64_t expected_num_sample_upper = uint64_t(1.1 * expected_num_samples);
    const double observedNumSamples = static_cast<double>(cumulative_sum_);
    const double observed_rate = observedNumSamples / t_elapsed.count() / num_subprocesses;

    LOG(INFO) << absl::StrFormat("Table sampling period: %d [ms].", table_period_ms);
    LOG(INFO) << absl::StrFormat("BPF sampling period: %d [ms].", bpf_period_ms);
    LOG(INFO) << absl::StrFormat("Number of processes: %d.", num_subprocesses);
    LOG(INFO) << absl::StrFormat("expected num samples: %d.", uint64_t(expected_num_samples));
    LOG(INFO) << absl::StrFormat("total samples: %d.", cumulative_sum_);
    LOG(INFO) << absl::StrFormat("elapsed time: %.1f [sec].", t_elapsed.count());
    LOG(INFO) << absl::StrFormat("expected sample rate: %.2f [Hz].", expected_rate);
    LOG(INFO) << absl::StrFormat("observed sample rate: %.2f [Hz].", observed_rate);

    // We expect to see a certain number of samples, but in practice
    // see fewer (maybe the CPU and Linux scheduler have other things to do!).
    // For this test, use an upper & lower bound with 10% allowed error band.
    const std::string err_msg = absl::StrFormat(
        "num sub-processes: %d, time: %.2f [sec.], rate: %.2f [Hz], observed_rate: %.2f [Hz]",
        num_subprocesses, t_elapsed.count(), expected_rate, observed_rate);
    EXPECT_GT(cumulative_sum_, expected_num_sample_lower) << err_msg;
    EXPECT_LT(cumulative_sum_, expected_num_sample_upper) << err_msg;

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
    // TODO(jps): This is extremely flaky on Jenkins. Please fix and re-enable.
    // EXPECT_LT(ratio, 2.0 + kRatioMargin);

    EXPECT_EQ(source_->stats().Get(PerfProfileConnector::StatKey::kLossHistoEvent), 0);
  }

  void ConsumeRecords() {
    const std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(columns_, tablets);
    PopulateColumnPtrs(columns_);
    auto target_row_idxs =
        FindRecordIdxMatchesPIDs(columns_, kStackTraceUPIDIdx, sub_processes_->pids());

    PopulateCumulativeSum(target_row_idxs);
    PopulateObservedStackTraces(target_row_idxs);
  }

  void PopulateColumnPtrs(const types::ColumnWrapperRecordBatch& columns) {
    trace_ids_column_ = columns[kStackTraceStackTraceIDIdx];
    stack_traces_column_ = columns[kStackTraceStackTraceStrIdx];
    counts_column_ = columns[kStackTraceCountIdx];
    column_ptrs_populated_ = true;
  }

  void RefreshContext(const absl::flat_hash_set<md::UPID>& upids) {
    absl::base_internal::SpinLockHolder lock(&perf_profiler_state_lock_);
    ctx_ = std::make_unique<StandaloneContext>(upids);
  }

  void StartTransferDataThread() {
    ASSERT_TRUE(source_ != nullptr);

    sampling_period_ = source_->SamplingPeriod();
    ASSERT_LT(sampling_period_, test_run_time_);

    // Create an initial empty context for use by the transfer data thread.
    const absl::flat_hash_set<md::UPID> empty_upid_set;
    ctx_ = std::make_unique<StandaloneContext>(empty_upid_set);

    transfer_data_thread_ = std::thread([this]() {
      transfer_enable_ = true;
      while (transfer_enable_) {
        {
          absl::base_internal::SpinLockHolder lock(&perf_profiler_state_lock_);
          source_->TransferData(ctx_.get());
        }
        std::this_thread::sleep_for(sampling_period_);
      }
    });
  }

  void StopTransferDataThread() {
    CHECK(transfer_data_thread_.joinable());
    transfer_enable_ = false;
    transfer_data_thread_.join();
    source_->TransferData(ctx_.get());
  }

  std::chrono::duration<double> RunTest() {
    const auto start_time = std::chrono::steady_clock::now();

    // While we sleep (here), the transfer data thread is running.
    // In that thread, TransferData() is invoked periodically.
    std::this_thread::sleep_for(test_run_time_);
    StopTransferDataThread();

    // We return the amount of time that we ran the test; it will be used to compute
    // the observed sample rate and the expected number of samples.
    return std::chrono::steady_clock::now() - start_time;
  }

  const std::chrono::seconds test_run_time_;
  std::chrono::milliseconds sampling_period_;
  std::atomic<bool> transfer_enable_ = false;
  std::thread transfer_data_thread_;
  absl::base_internal::SpinLock perf_profiler_state_lock_;
  std::unique_ptr<PerfProfileConnector> source_;
  std::unique_ptr<PerfProfilerTestSubProcesses> sub_processes_;
  std::unique_ptr<StandaloneContext> ctx_;
  DataTable data_table_;

  bool column_ptrs_populated_ = false;
  std::shared_ptr<types::ColumnWrapper> trace_ids_column_;
  std::shared_ptr<types::ColumnWrapper> stack_traces_column_;
  std::shared_ptr<types::ColumnWrapper> counts_column_;

  uint64_t cumulative_sum_ = 0;
  absl::flat_hash_map<std::string, uint64_t> histo_;

  types::ColumnWrapperRecordBatch columns_;

  // To reduce variance in results, we add more run-time or add sub-processes:
  static constexpr uint64_t kNumSubProcs = 4;
  static constexpr double kRatioMargin = 0.5;
};

TEST_F(PerfProfileBPFTest, PerfProfilerGoTest) {
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
  RefreshContext(sub_processes_->upids());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> t_elapsed = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(CheckExpectedCounts(histo_, kNumSubProcs, t_elapsed, key1x, key2x));
}

TEST_F(PerfProfileBPFTest, PerfProfilerCppTest) {
  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // The target app is written such that key2x uses twice the CPU time as key1x.
  constexpr std::string_view key2x = "main;fib52();fib(unsigned long)";
  constexpr std::string_view key1x = "main;fib27();fib(unsigned long)";

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());
  RefreshContext(sub_processes_->upids());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> t_elapsed = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedCounts(KeepNLeafSyms(3, histo_), kNumSubProcs, t_elapsed, key1x, key2x));
}

// TODO(jps/oazizi): This test is flaky.
TEST_F(PerfProfileBPFTest, DISABLED_GraalVM_AOT_Test) {
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
  RefreshContext(sub_processes_->upids());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> t_elapsed = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedCounts(KeepNLeafSyms(1, histo_), kNumSubProcs, t_elapsed, key1x, key2x));
}

TEST_P(PerfProfileBPFTest, PerfProfilerJavaTest) {
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
  RefreshContext(sub_processes_->upids());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> t_elapsed = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedCounts(KeepNLeafSyms(1, histo_), kNumSubProcs, t_elapsed, key1x, key2x));

  // Now we will test agent cleanup, specifically whether the aritfacts directory is removed.
  // We will construct a list of artifacts paths that we expect,
  // then kill all the subprocesses,
  // and expect that all the artifacts paths are (as a result) removed.
  std::vector<std::filesystem::path> artifacts_paths;

  // Consruct the names of the artifacts paths and expect that they exist.
  for (const auto& upid : sub_processes_->struct_upids()) {
    const auto artifacts_path = java::StirlingArtifactsPath(upid);
    EXPECT_TRUE(fs::Exists(artifacts_path));
    if (fs::Exists(artifacts_path)) {
      artifacts_paths.push_back(artifacts_path);
    }
  }
  EXPECT_THAT(artifacts_paths, SizeIs(kNumSubProcs));

  // Kill the subprocs.
  sub_processes_->KillAll();

  // Inside of PerfProfileConnector, we need the list of deleted upids to match our original
  // list of upids based on our subprocs.
  // For that to happen, here, we reset the context so that it has no UPIDs.
  // The ProcTracker (inside of PerfProfileConnector) will take the difference
  // between the previous list of upids (our subprocs) and the current list of upids (empty)
  // to find a list of deleted upids.
  const absl::flat_hash_set<md::UPID> empty_upid_set;
  RefreshContext(empty_upid_set);

  // Run transfer data so that cleanup is kicked off in the perf profile source connector.
  // The deleted upids list that is inferred will match our original upid list.
  source_->TransferData(ctx_.get());

  // Expect that that the artifacts paths have been removed.
  EXPECT_THAT(artifacts_paths, Each(Not(PathExists())));
}

TEST_F(PerfProfileBPFTest, TestOutOfContext) {
  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // Start target apps & create the connector context using the sub-process upids.
  sub_processes_ = std::make_unique<CPUPinnedSubProcesses>(bazel_app_path);
  ASSERT_NO_FATAL_FAILURE(sub_processes_->StartAll());

  // Use an empty connector context.
  ctx_ = std::make_unique<StandaloneContext>(absl::flat_hash_set<md::UPID>());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  RunTest();

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

INSTANTIATE_TEST_SUITE_P(PerfProfileJavaTests, PerfProfileBPFTest,
                         ::testing::ValuesIn(GetJavaImagePaths()));

}  // namespace stirling
}  // namespace px
