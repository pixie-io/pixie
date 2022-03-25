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

#include <sys/sysinfo.h>

#include <absl/strings/substitute.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/testing/common.h"

DEFINE_uint32(test_run_time, 90, "Number of seconds to run the test.");
DECLARE_bool(stirling_profiler_java_symbols);

namespace px {
namespace stirling {

using ::px::stirling::testing::FindRecordIdxMatchesPIDs;
using ::px::testing::BazelBinTestFilePath;
using ::testing::Gt;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class CPUPinnedBinaryRunner {
 public:
  void Run(const std::string& binary_path, const uint64_t cpu_idx) {
    // Run the sub-process & pin it to a CPU.
    const std::string kTasksetBinPath = "/usr/bin/taskset";
    ASSERT_TRUE(fs::Exists(binary_path));
    ASSERT_TRUE(fs::Exists(kTasksetBinPath));
    ASSERT_OK(sub_process_.Start({kTasksetBinPath, "-c", std::to_string(cpu_idx), binary_path}));
  }
  ~CPUPinnedBinaryRunner() { sub_process_.Kill(); }
  int pid() const { return sub_process_.child_pid(); }
  void Kill() { sub_process_.Kill(); }

 private:
  SubProcess sub_process_;
};

absl::flat_hash_set<md::UPID> ToUPIDs(const std::vector<CPUPinnedBinaryRunner>& processes) {
  absl::flat_hash_set<md::UPID> upids;
  system::ProcParser proc_parser(system::Config::GetInstance());
  for (const auto& p : processes) {
    StatusOr<uint64_t> ts = proc_parser.GetPIDStartTimeTicks(p.pid());
    if (!ts.ok()) {
      LOG(ERROR) << absl::Substitute("Could not find start_time of PID=$0", p.pid());
      continue;
    }
    upids.emplace(0, p.pid(), ts.ValueOr(-1));
  }
  return upids;
}

class PerfProfileBPFTest : public ::testing::Test {
 public:
  PerfProfileBPFTest()
      : test_run_time_(FLAGS_test_run_time), data_table_(/*id*/ 0, kStackTraceTable) {}

 protected:
  void SetUp() override {
    FLAGS_stirling_profiler_java_symbols = true;
    source_ = PerfProfileConnector::Create("perf_profile_connector");
    ASSERT_OK(source_->Init());
    ASSERT_LT(source_->SamplingPeriod(), test_run_time_);
  }

  void TearDown() override { ASSERT_OK(source_->Stop()); }

  // BazelXXTestAppPath() are helpers to build & return the bazelified path to
  // one of the toy apps used by perf profiler testing.
  std::filesystem::path BazelCCTestAppPath(const std::filesystem::path& app_name) {
    const std::filesystem::path kToyAppsPath =
        "src/stirling/source_connectors/perf_profiler/testing/cc";
    const std::filesystem::path app_path = fs::JoinPath({&kToyAppsPath, &app_name});
    const std::filesystem::path bazel_app_path = BazelBinTestFilePath(app_path);
    return bazel_app_path;
  }

  std::filesystem::path BazelGoTestAppPath(const std::filesystem::path& app_name) {
    const std::string sub_path_str = absl::Substitute(
        "src/stirling/source_connectors/perf_profiler/testing/go/$0_", app_name.string());
    const std::filesystem::path sub_path = sub_path_str;
    const std::filesystem::path app_path = fs::JoinPath({&sub_path, &app_name});
    const std::filesystem::path bazel_app_path = BazelBinTestFilePath(app_path);
    return bazel_app_path;
  }

  std::filesystem::path BazelJavaTestAppPath(const std::filesystem::path& app_name) {
    const std::filesystem::path kToyAppsPath =
        "src/stirling/source_connectors/perf_profiler/testing/java";
    const std::filesystem::path app_path = fs::JoinPath({&kToyAppsPath, &app_name});
    const std::filesystem::path bazel_app_path = BazelBinTestFilePath(app_path);
    return bazel_app_path;
  }

  // This is templatized because we anticipate having more than one kind of binary runner,
  // i.e. because future test applications will be threaded, so the CPU pinning will be
  // done "inside" of the test app.
  template <typename T>
  std::vector<T> StartSubProcesses(const std::filesystem::path& app_path) {
    // Before poking the subProcess.Run() method, we need the vector to be
    // fully populated (or else vector re-sizing will want to make copies).
    // Copying an already running sub-process kills the sub-process.
    // Using kNumSubProcesses as a constructor arg. pre-populates the vector
    // with default constructor initialized values.
    std::vector<T> sub_processes(kNumSubProcesses);

    for (uint32_t cpu_idx = 0; cpu_idx < kNumSubProcesses; ++cpu_idx) {
      sub_processes[cpu_idx].Run(app_path, cpu_idx);
    }

    // Give test apps a little time to start running (necessary for Java, at least).
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Create a connector context that has only the UPIDs of interest, for this test.
    ctx_ = std::make_unique<TestContext>(ToUPIDs(sub_processes));
    target_pids_ = GetSubProcessPids(sub_processes);

    return sub_processes;
  }

  void PopulateObservedStackTraces(const std::vector<size_t>& target_row_idxs) {
    // Just check that the test author populated the necessary,
    // and did not corrupt the cumulative sum already.
    ASSERT_TRUE(column_ptrs_populated_);

    for (const auto row_idx : target_row_idxs) {
      // Build the histogram of observed stack traces here:
      // Also, track the cumulative sum (or total number of samples).
      const std::string stack_trace_str = stack_traces_column_->Get<types::StringValue>(row_idx);
      const std::vector<std::string_view> symbols = absl::StrSplit(stack_trace_str, ";");
      const std::string_view leaf_symbol = symbols.back();

      const int64_t count = counts_column_->Get<types::Int64Value>(row_idx).val;
      observed_stack_traces_[stack_trace_str] += count;
      observed_leaf_symbols_[leaf_symbol] += count;
    }

    // TODO(jps): bring in a 3rd party library for colorization. e.g., one of the following:
    // https://github.com/agauniyal/rang
    // https://github.com/ikalnytskyi/termcolor
    auto makecolor = [](const auto n) { return absl::StrFormat("\x1b[38;5;$%dm", n); };
    auto reset = []() { return "\x1b[0m"; };
    VLOG(1) << std::endl;
    for (const auto& [key, val] : observed_stack_traces_) {
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
                           const std::chrono::duration<double> elapsed_time,
                           const std::string_view key1x, const std::string_view key2x) {
    const uint64_t table_period_ms = source_->SamplingPeriod().count();
    const uint64_t bpf_period_ms = source_->StackTraceSamplingPeriod().count();
    const double expected_rate = 1000.0 / static_cast<double>(bpf_period_ms);
    const double expected_num_samples = num_subprocesses * elapsed_time.count() * expected_rate;
    const uint64_t expected_num_sample_lower = uint64_t(0.9 * expected_num_samples);
    const uint64_t expected_num_sample_upper = uint64_t(1.1 * expected_num_samples);
    const double observedNumSamples = static_cast<double>(cumulative_sum_);
    const double observed_rate = observedNumSamples / elapsed_time.count() / num_subprocesses;

    LOG(INFO) << absl::StrFormat("Table sampling period: %d [ms].", table_period_ms);
    LOG(INFO) << absl::StrFormat("BPF sampling period: %d [ms].", bpf_period_ms);
    LOG(INFO) << absl::StrFormat("Number of processes: %d.", num_subprocesses);
    LOG(INFO) << absl::StrFormat("expected num samples: %d.", uint64_t(expected_num_samples));
    LOG(INFO) << absl::StrFormat("total samples: %d.", cumulative_sum_);
    LOG(INFO) << absl::StrFormat("elapsed time: %.1f [sec].", elapsed_time.count());
    LOG(INFO) << absl::StrFormat("expected sample rate: %.2f [Hz].", expected_rate);
    LOG(INFO) << absl::StrFormat("observed sample rate: %.2f [Hz].", observed_rate);

    // We expect to see a certain number of samples, but in practice
    // see fewer (maybe the CPU and Linux scheduler have other things to do!).
    // For this test, use an upper & lower bound with 10% allowed error band.
    const std::string err_msg = absl::StrFormat(
        "num sub-processes: %d, time: %.2f [sec.], rate: %.2f [Hz], observed_rate: %.2f [Hz]",
        num_subprocesses, elapsed_time.count(), expected_rate, observed_rate);
    EXPECT_GT(cumulative_sum_, expected_num_sample_lower) << err_msg;
    EXPECT_LT(cumulative_sum_, expected_num_sample_upper) << err_msg;

    char const* const missing_key_msg = "Could not find required symbol or stack trace: $0.";
    ASSERT_TRUE(counts.find(key1x) != counts.end()) << absl::Substitute(missing_key_msg, key1x);
    ASSERT_TRUE(counts.find(key2x) != counts.end()) << absl::Substitute(missing_key_msg, key2x);

    const double key1x_count = counts.at(key1x);
    const double key2x_count = counts.at(key2x);
    const double ratio = key2x_count / key1x_count;

    // We expect the ratio of key2x:key1x to be approx. 2:1.
    // TODO(jps): Can we tighten the margin? e.g. by increasing sampling frequency.
    LOG(INFO) << absl::StrFormat("key2x: %s.", key2x);
    LOG(INFO) << absl::StrFormat("key1x: %s.", key1x);
    LOG(INFO) << absl::StrFormat("key2x count: %d.", static_cast<uint64_t>(key2x_count));
    LOG(INFO) << absl::StrFormat("key1x count: %d.", static_cast<uint64_t>(key1x_count));
    LOG(INFO) << absl::StrFormat("ratio: %.2fx.", ratio);
    EXPECT_GT(ratio, 2.0 - kRatioMargin);
    EXPECT_LT(ratio, 2.0 + kRatioMargin);

    EXPECT_EQ(source_->stats().Get(PerfProfileConnector::StatKey::kLossHistoEvent), 0);
  }

  template <typename T>
  std::vector<int> GetSubProcessPids(const std::vector<T>& sub_processes) {
    std::vector<int> pids;
    for (const auto& sub_process : sub_processes) {
      pids.push_back(sub_process.pid());
    }
    return pids;
  }

  template <typename T>
  std::vector<struct upid_t> GetSubProcessUPIDs(const std::vector<T>& sub_processes) {
    const std::vector<int> pids_vec = GetSubProcessPids(sub_processes);
    const std::set<int> pids(pids_vec.begin(), pids_vec.end());
    const auto& md_upids = ctx_->GetUPIDs();
    std::vector<struct upid_t> upids;

    for (const auto upid : md_upids) {
      if (pids.find(upid.pid()) != pids.end()) {
        upids.push_back({{upid.pid()}, static_cast<uint64_t>(upid.start_ts())});
      }
    }
    return upids;
  }

  void ConsumeRecords() {
    const std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
    ASSERT_NOT_EMPTY_AND_GET_RECORDS(columns_, tablets);
    PopulateColumnPtrs(columns_);
    auto target_row_idxs = FindRecordIdxMatchesPIDs(columns_, kStackTraceUPIDIdx, target_pids_);

    PopulateCumulativeSum(target_row_idxs);
    PopulateObservedStackTraces(target_row_idxs);
  }

  void PopulateColumnPtrs(const types::ColumnWrapperRecordBatch& columns) {
    trace_ids_column_ = columns[kStackTraceStackTraceIDIdx];
    stack_traces_column_ = columns[kStackTraceStackTraceStrIdx];
    counts_column_ = columns[kStackTraceCountIdx];
    column_ptrs_populated_ = true;
  }

  std::chrono::duration<double> RunTest() {
    const std::chrono::milliseconds t_sleep = source_->SamplingPeriod();
    const auto start_time = std::chrono::steady_clock::now();
    const auto stop_time = start_time + test_run_time_;

    // Continuously poke Stirling TransferData() using the underlying schema periodicity;
    // break from this loop when the elapsed time exceeds the targeted run time.
    while (std::chrono::steady_clock::now() < stop_time) {
      source_->TransferData(ctx_.get(), data_tables_);
      std::this_thread::sleep_for(t_sleep);
    }
    source_->TransferData(ctx_.get(), data_tables_);

    // We return the amount of time that we ran the test; it will be used to compute
    // the observed sample rate and the expected number of samples.
    return std::chrono::steady_clock::now() - start_time;
  }

  const std::chrono::seconds test_run_time_;
  std::unique_ptr<PerfProfileConnector> source_;
  std::unique_ptr<TestContext> ctx_;
  std::vector<int> target_pids_;
  DataTable data_table_;
  const std::vector<DataTable*> data_tables_{&data_table_};

  bool column_ptrs_populated_ = false;
  std::shared_ptr<types::ColumnWrapper> trace_ids_column_;
  std::shared_ptr<types::ColumnWrapper> stack_traces_column_;
  std::shared_ptr<types::ColumnWrapper> counts_column_;

  uint64_t cumulative_sum_ = 0;
  absl::flat_hash_map<std::string, uint64_t> observed_stack_traces_;
  absl::flat_hash_map<std::string, uint64_t> observed_leaf_symbols_;

  types::ColumnWrapperRecordBatch columns_;

  // To reduce variance in results, we add more run-time or add sub-processes:
  static constexpr uint64_t kNumSubProcesses = 4;
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
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path);

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> elapsed_time = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedCounts(observed_stack_traces_, kNumSubProcesses, elapsed_time, key1x, key2x));
}

TEST_F(PerfProfileBPFTest, PerfProfilerCppTest) {
  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // The target app is written such that key2x uses twice the CPU time as key1x.
  constexpr std::string_view key2x = "__libc_start_main;main;fib52();fib(unsigned long)";
  constexpr std::string_view key1x = "__libc_start_main;main;fib27();fib(unsigned long)";

  // Start target apps & create the connector context using the sub-process upids.
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path);

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> elapsed_time = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedCounts(observed_stack_traces_, kNumSubProcesses, elapsed_time, key1x, key2x));
}

TEST_F(PerfProfileBPFTest, PerfProfilerJavaTest) {
  const std::filesystem::path bazel_app_path = BazelJavaTestAppPath("fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // The target app is written such that key2x uses twice the CPU time as key1x.
  // For Java, we will match only the leaf symbol because we cannot predict the full stack trace.
  constexpr std::string_view key2x = "[j] long JavaFib::fib52()";
  constexpr std::string_view key1x = "[j] long JavaFib::fib27()";

  // Start target apps & create the connector context using the sub-process upids.
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path);

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  const std::chrono::duration<double> elapsed_time = RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());

  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedCounts(observed_leaf_symbols_, kNumSubProcesses, elapsed_time, key1x, key2x));

  // Now we will test agent cleanup, specifically whether the aritfacts directory is removed.
  // We will construt a list of artifacts paths that we expect,
  // then kill all the subprocesses,
  // and expect that all the artifacts paths are (as a result) removed.
  std::vector<std::filesystem::path> artifacts_paths;

  // Get the UPIDs of our subprocs.
  const auto upids = GetSubProcessUPIDs(sub_processes);

  // Consruct the names of the artifacts paths and expect that they exist.
  for (const auto& upid : upids) {
    ASSERT_OK_AND_ASSIGN(const auto artifacts_path, java::ResolveHostArtifactsPath(upid));
    EXPECT_TRUE(fs::Exists(artifacts_path));
    if (fs::Exists(artifacts_path)) {
      artifacts_paths.push_back(artifacts_path);
    }
  }
  EXPECT_EQ(artifacts_paths.size(), kNumSubProcesses);

  // Kill the subprocs.
  for (auto& proc : sub_processes) {
    proc.Kill();
  }

  // Inside of PerfProfileConnector, we need the list of deleted upids to match our original
  // list of upids based on our subprocs.
  // For that to happen, here, we reset the context so that it has no UPIDs.
  // The ProcTracker (inside of PerfProfileConnector) will take the difference
  // between the previous list of upids (our subprocs) and the current list of upids (empty)
  // to find a list of deleted upids.
  const absl::flat_hash_set<md::UPID> empty_upid_set;
  ctx_ = std::make_unique<TestContext>(empty_upid_set);

  // Run transfer data so that cleanup is kicked off in the perf profile source connector.
  // The deleted upids list that is inferred will match our original upid list.
  source_->TransferData(ctx_.get(), data_tables_);

  // Expect that that the artifacts paths have been removed.
  for (const auto& artifacts_path : artifacts_paths) {
    EXPECT_FALSE(fs::Exists(artifacts_path)) << artifacts_path;
  }
}

TEST_F(PerfProfileBPFTest, TestOutOfContext) {
  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");
  ASSERT_TRUE(fs::Exists(bazel_app_path)) << absl::StrFormat("Missing: %s.", bazel_app_path);

  // Start target apps & create the connector context using the sub-process upids.
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path);

  // Replace the populated connector context with one that is empty.
  ctx_ = std::make_unique<TestContext>(absl::flat_hash_set<md::UPID>());

  // Allow target apps to run, and periodically call transfer data on perf profile connector.
  RunTest();

  // Pull the data from the perf profile connector into this test case.
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
}

}  // namespace stirling
}  // namespace px
