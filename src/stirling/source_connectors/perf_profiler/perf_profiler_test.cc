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

#include <sys/sysinfo.h>

#include <absl/strings/substitute.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/testing/common.h"

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
    ASSERT_OK(fs::Exists(binary_path));
    ASSERT_OK(fs::Exists(kTasksetBinPath));
    ASSERT_OK(sub_process_.Start({kTasksetBinPath, "-c", std::to_string(cpu_idx), binary_path}));
  }

  ~CPUPinnedBinaryRunner() { sub_process_.Kill(); }
  int pid() const { return sub_process_.child_pid(); }

 private:
  SubProcess sub_process_;
};

class PerfProfileBPFTest : public ::testing::Test {
 public:
  PerfProfileBPFTest() : data_table_(/*id*/ 0, kStackTraceTable) {}

 protected:
  void SetUp() override {
    source_ = PerfProfileConnector::Create("perf_profile_connector");
    ASSERT_OK(source_->Init());
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

  // This is templatized because we anticipate having more than one kind of binary runner,
  // i.e. because future test applications will be threaded, so the CPU pinning will be
  // done "inside" of the test app.
  template <typename T>
  std::vector<T> StartSubProcesses(const std::filesystem::path& app_path, const uint32_t test_idx) {
    // Before poking the subProcess.Run() method, we need the vector to be
    // fully populated (or else vector re-sizing will want to make copies).
    // Copying an already running sub-process kills the sub-process.
    // Using kNumSubProcesses as a constructor arg. pre-populates the vector
    // with default constructor initialized values.
    std::vector<T> sub_processes(kNumSubProcesses);

    for (uint64_t sub_process_idx = 0; sub_process_idx < kNumSubProcesses; ++sub_process_idx) {
      const uint32_t cpu_idx = kNumSubProcesses * test_idx + sub_process_idx;
      sub_processes[sub_process_idx].Run(app_path, cpu_idx);
    }

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
      const int64_t count = counts_column_->Get<types::Int64Value>(row_idx).val;
      observed_stack_traces_[stack_trace_str] += count;
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

  void CheckExpectedStackTraceCounts(const ssize_t num_subprocesses,
                                     const std::chrono::duration<double> elapsed_time,
                                     const std::string& key1x, const std::string& key2x) {
    const uint64_t kBPFSamplingPeriodMillis = PerfProfileConnector::kBPFSamplingPeriod.count();
    const double expected_rate = 1000.0 / static_cast<double>(kBPFSamplingPeriodMillis);
    const double expected_num_samples = num_subprocesses * elapsed_time.count() * expected_rate;
    const uint64_t expected_num_sample_lower = uint64_t(0.9 * expected_num_samples);
    const uint64_t expected_num_sample_upper = uint64_t(1.1 * expected_num_samples);
    const double observedNumSamples = static_cast<double>(cumulative_sum_);
    const double observed_rate = observedNumSamples / elapsed_time.count() / num_subprocesses;

    LOG(INFO) << absl::StrFormat("expected num samples: %d", uint64_t(expected_num_samples));
    LOG(INFO) << absl::StrFormat("total samples: %d", cumulative_sum_);
    LOG(INFO) << absl::StrFormat("elapsed time: %.1f [sec]", elapsed_time.count());
    LOG(INFO) << absl::StrFormat("expected sample rate: %.2f [Hz]", expected_rate);
    LOG(INFO) << absl::StrFormat("observed sample rate: %.2f [Hz]", observed_rate);

    // We expect to see a certain number of samples, but in practice
    // see fewer (maybe the CPU and Linux scheduler have other things to do!).
    // For this test, use an upper & lower bound with 10% allowed error band.
    const std::string err_msg = absl::StrFormat(
        "num sub-processes: %d, time: %.2f [sec.], rate: %.2f [Hz], observed_rate: %.2f [Hz]",
        num_subprocesses, elapsed_time.count(), expected_rate, observed_rate);
    EXPECT_GT(cumulative_sum_, expected_num_sample_lower) << err_msg;
    EXPECT_LT(cumulative_sum_, expected_num_sample_upper) << err_msg;

    const double num_1x_samples = observed_stack_traces_[key1x];
    const double num_2x_samples = observed_stack_traces_[key2x];
    const double ratio = num_2x_samples / num_1x_samples;

    // We expect the ratio of fib52:fib27 to be approx. 2:1;
    // or sqrt, or something else that was in the toy test app.
    // TODO(jps): Increase sampling frequency and then tighten this margin.
    constexpr double kMargin = 0.5;
    EXPECT_GT(ratio, 2.0 - kMargin);
    EXPECT_LT(ratio, 2.0 + kMargin);
  }

  template <typename T>
  std::vector<size_t> GetTargetRowIdxs(const std::vector<T>& sub_processes) {
    auto GetSubProcessPids = [&]() {
      std::vector<int> pids;
      for (const auto& sub_process : sub_processes) {
        pids.push_back(sub_process.pid());
      }
      return pids;
    };

    const std::vector<int> pids = GetSubProcessPids();
    return FindRecordIdxMatchesPIDs(columns_, kStackTraceUPIDIdx, pids);
  }

  void ConsumeRecords() {
    const std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
    ASSERT_EQ(tablets.size(), 1);

    columns_ = tablets[0].records;

    PopulateColumnPtrs(columns_);
  }

  void PopulateColumnPtrs(const types::ColumnWrapperRecordBatch& columns) {
    trace_ids_column_ = columns[kStackTraceStackTraceIDIdx];
    stack_traces_column_ = columns[kStackTraceStackTraceStrIdx];
    counts_column_ = columns[kStackTraceCountIdx];
    column_ptrs_populated_ = true;
  }

  std::chrono::duration<double> RunTest(const std::chrono::seconds test_run_time) {
    constexpr std::chrono::milliseconds t_sleep = PerfProfileConnector::kSamplingPeriod;
    const auto start_time = std::chrono::steady_clock::now();
    const auto stop_time = start_time + test_run_time;

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

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<StandaloneContext> ctx_;
  DataTable data_table_;
  const std::vector<DataTable*> data_tables_{&data_table_};

  bool column_ptrs_populated_ = false;
  std::shared_ptr<types::ColumnWrapper> trace_ids_column_;
  std::shared_ptr<types::ColumnWrapper> stack_traces_column_;
  std::shared_ptr<types::ColumnWrapper> counts_column_;

  uint64_t cumulative_sum_ = 0;
  absl::flat_hash_map<std::string, uint64_t> observed_stack_traces_;

  types::ColumnWrapperRecordBatch columns_;

  // To reduce variance in results, we add more run-time or add sub-processes:
  static constexpr uint64_t kNumSubProcesses = 4;
};

TEST_F(PerfProfileBPFTest, PerfProfilerGoTest) {
  // Needs to be unique across test fixtures because we use this to map
  // into CPU index. If non-unique, two or more different test fixtures
  // will run their toy apps. on the same CPU.
  constexpr uint32_t kTestIdx = 1;

  const std::filesystem::path bazel_app_path = BazelGoTestAppPath("profiler_test_app_sqrt_go");

  // The toy test app. should be written such that we can expect one stack trace
  // twice as often as another.
  std::string key2x = "runtime.goexit;runtime.main;main.main;main.sqrtOf1e39;main.sqrt";
  std::string key1x = "runtime.goexit;runtime.main;main.main;main.sqrtOf1e18;main.sqrt";

  // Start they toy apps as sub-processes, then,
  // for a certain amount of time (kTestRunTime), collect data using RunTest().
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path, kTestIdx);

  // We wait until here to create the connector context, i.e. so that perf_profile_connector
  // finds the upids that belong to the sub-processes that we have just created.
  ctx_ = std::make_unique<StandaloneContext>();

  const std::chrono::duration<double> elapsed_time = RunTest(std::chrono::seconds(120));

  // Pull the data into this test (as columns_) using ConsumeRecords(), and
  // find the row indices that belong to our sub-processes using GetTargetRowIdxs().
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
  const std::vector<size_t> target_row_idxs = GetTargetRowIdxs(sub_processes);

  // Populate the cumulative sum & the observed stack traces histo,
  // then check observed vs. expected stack traces key set:
  ASSERT_NO_FATAL_FAILURE(PopulateCumulativeSum(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(PopulateObservedStackTraces(target_row_idxs));
  EXPECT_THAT(observed_stack_traces_, ::testing::Contains(Pair(key1x, Gt(0))));
  EXPECT_THAT(observed_stack_traces_, ::testing::Contains(Pair(key2x, Gt(0))));
  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedStackTraceCounts(kNumSubProcesses, elapsed_time, key1x, key2x));
}

TEST_F(PerfProfileBPFTest, PerfProfilerCppTest) {
  // Needs to be unique across test fixtures because we use this to map
  // into CPU index. If non-unique, two or more different test fixtures
  // will run their toy apps. on the same CPU.
  constexpr uint32_t kTestIdx = 0;

  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");

  // The toy test app. should be written such that we can expect one stack trace
  // twice as often as another.
  std::string key2x = "__libc_start_main;main;fib52();fib(unsigned long)";
  std::string key1x = "__libc_start_main;main;fib27();fib(unsigned long)";

  // Start they toy apps as sub-processes, then,
  // for a certain amount of time, collect data using RunTest().
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path, kTestIdx);

  // We wait until here to create the connector context, i.e. so that perf_profile_connector
  // finds the upids that belong to the sub-processes that we have just created.
  ctx_ = std::make_unique<StandaloneContext>();

  const std::chrono::duration<double> elapsed_time = RunTest(std::chrono::seconds(120));

  // Pull the data into this test (as columns_) using ConsumeRecords(), and
  // find the row indices that belong to our sub-processes using GetTargetRowIdxs().
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
  const std::vector<size_t> target_row_idxs = GetTargetRowIdxs(sub_processes);

  // Populate the cumulative sum & the observed stack traces histo,
  // then check observed vs. expected stack traces key set:
  ASSERT_NO_FATAL_FAILURE(PopulateCumulativeSum(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(PopulateObservedStackTraces(target_row_idxs));
  EXPECT_THAT(observed_stack_traces_, ::testing::Contains(Pair(key1x, Gt(0))));
  EXPECT_THAT(observed_stack_traces_, ::testing::Contains(Pair(key2x, Gt(0))));
  ASSERT_NO_FATAL_FAILURE(
      CheckExpectedStackTraceCounts(kNumSubProcesses, elapsed_time, key1x, key2x));
}

TEST_F(PerfProfileBPFTest, TestOutOfContext) {
  // Needs to be unique across test fixtures because we use this to map
  // into CPU index. If non-unique, two or more different test fixtures
  // will run their toy apps. on the same CPU.
  constexpr uint32_t kTestIdx = 2;

  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");

  // For this test case, we create the connector context *before*
  // starting sub-processes. For this reason, the perf_profile_connector
  // will consider the sub-processes as "out-of-context" and not symbolize them.
  ctx_ = std::make_unique<StandaloneContext>();

  // Start they toy apps as sub-processes, then,
  // for a certain amount of time, collect data using RunTest().
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path, kTestIdx);
  RunTest(std::chrono::seconds(30));

  // Pull the data into this test (as columns_) using ConsumeRecords(), and
  // find the row indices that belong to our sub-processes using GetTargetRowIdxs().
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
  const std::vector<size_t> target_row_idxs = GetTargetRowIdxs(sub_processes);

  // Populate the cumulative sum & the observed stack traces histo,
  // then check observed vs. expected stack traces key set:
  ASSERT_NO_FATAL_FAILURE(PopulateCumulativeSum(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(PopulateObservedStackTraces(target_row_idxs));
}

}  // namespace stirling
}  // namespace px
