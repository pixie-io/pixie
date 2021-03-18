#include <sys/sysinfo.h>

#include <absl/strings/substitute.h>
#include <gtest/gtest.h>

#include "src/common/base/base.h"
#include "src/common/exec/subprocess.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/perf_profile_connector.h"
#include "src/stirling/source_connectors/perf_profiler/stack_traces_table.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

using ::pl::testing::BazelBinTestFilePath;
using testing::FindRecordIdxMatchesPIDs;

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
  PerfProfileBPFTest() : data_table_(kStackTraceTable) {}

 protected:
  void SetUp() override {
    FLAGS_stirling_profiler_test_mode = true;
    source_ = PerfProfileConnector::Create("perf_profile_connector");
    ASSERT_OK(source_->Init());
    ctx_ = std::make_unique<StandaloneContext>();
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

  void CheckThatAllColumnsHaveSameNumRows(const types::ColumnWrapperRecordBatch& columns) {
    const size_t num_rows = columns[kStackTraceTimeIdx]->Size();
    ASSERT_THAT(columns, ::testing::Each(testing::ColWrapperSizeIs(num_rows)));
  }

  void CheckStackTraceIDsInvariance() {
    // Just check that the test author populated the necessary.
    ASSERT_TRUE(column_ptrs_populated_);
    const size_t num_rows = stack_traces_column_->Size();

    // A map to track stack-trace-id to symbolic-stack-trace invariance:
    absl::flat_hash_map<int64_t, std::string_view> id_to_symbolic_repr_map;
    uint64_t num_stack_trace_ids_checked = 0;

    for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
      const int64_t stack_trace_id = trace_ids_column_->Get<types::Int64Value>(row_idx).val;
      const std::string_view stack_trace = stack_traces_column_->Get<types::StringValue>(row_idx);

      if (id_to_symbolic_repr_map.contains(stack_trace_id)) {
        // If we've seen a certain stack-trace-id before,
        // check that it's symoblic representation is the same as before.
        EXPECT_EQ(id_to_symbolic_repr_map[stack_trace_id], stack_trace);
        ++num_stack_trace_ids_checked;
      } else {
        // First time we've seen this stack-trace-id: just record it.
        id_to_symbolic_repr_map[stack_trace_id] = stack_trace;
      }
    }

    // Surely we did some checking of stack-trace-id:symbolic-stack-trace invariance;
    // check that we did.
    LOG(INFO) << "num_stack_trace_ids_checked: " << num_stack_trace_ids_checked;
    EXPECT_GT(num_stack_trace_ids_checked, 0);
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

  void CheckExpectedVsObservedStackTraces() {
    for (const auto& expected_stack_trace : expected_stack_traces_) {
      // Check that we observed all the stack traces that we expected:
      const bool stack_trace_found = observed_stack_traces_.contains(expected_stack_trace);
      EXPECT_TRUE(stack_trace_found);
      if (stack_trace_found) {
        const uint64_t count = observed_stack_traces_[expected_stack_trace];
        EXPECT_GT(count, 0) << expected_stack_trace;
        LOG(INFO) << expected_stack_trace << ": " << count;
      }
    }
  }

  void CheckExpectedStackTraceCounts(const ssize_t num_subprocesses,
                                     const std::chrono::duration<double> elapsed_time) {
    const uint64_t kBPFSmaplingPeriodMillis = PerfProfileConnector::BPFSamplingPeriodMillis();
    const double expected_rate = 1000.0 / static_cast<double>(kBPFSmaplingPeriodMillis);
    const double expected_num_samples = num_subprocesses * elapsed_time.count() * expected_rate;
    const uint64_t expected_num_sample_lower = uint64_t(0.9 * expected_num_samples);
    const uint64_t expected_num_sample_upper = uint64_t(1.1 * expected_num_samples);
    const double obsevedNumSamples = static_cast<double>(cumulative_sum_);
    const double observed_rate = obsevedNumSamples / elapsed_time.count() / num_subprocesses;

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

    const double num_2x_samples = observed_stack_traces_[key2x_];
    const double num_1x_samples = observed_stack_traces_[key1x_];
    const double ratio = num_2x_samples / num_1x_samples;

    // We expect the ratio of fib52:fib27 to be approx. 2:1;
    // or sqrt, or something else that was in the toy test app.
    LOG(INFO) << "ratio of 2x samples to 1x samples: " << ratio;
    EXPECT_GT(ratio, 1.85);
    EXPECT_LT(ratio, 2.15);
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

    CheckThatAllColumnsHaveSameNumRows(columns_);
    PopulateColumnPtrs(columns_);
  }

  void PopulateColumnPtrs(const types::ColumnWrapperRecordBatch& columns) {
    trace_ids_column_ = columns[kStackTraceStackTraceIDIdx];
    stack_traces_column_ = columns[kStackTraceStackTraceStrIdx];
    counts_column_ = columns[kStackTraceCountIdx];
    column_ptrs_populated_ = true;
  }

  std::chrono::duration<double> RunTest() {
    constexpr std::chrono::milliseconds t_sleep = kStackTraceTableSamplingPeriod;
    const auto start_time = std::chrono::steady_clock::now();
    const auto stop_time = start_time + kTestRunTime;

    // Continuously poke Stirling TransferData() using the underlying schema periodicity;
    // break from this loop when the elapsed time exceeds the targeted run time.
    while (std::chrono::steady_clock::now() < stop_time) {
      source_->TransferData(ctx_.get(), PerfProfileConnector::kPerfProfileTableNum, &data_table_);
      std::this_thread::sleep_for(t_sleep);
    }
    source_->TransferData(ctx_.get(), PerfProfileConnector::kPerfProfileTableNum, &data_table_);

    // We return the amount of time that we ran the test; it will be used to compute
    // the observed sample rate and the expected number of samples.
    return std::chrono::steady_clock::now() - start_time;
  }

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<StandaloneContext> ctx_;
  DataTable data_table_;

  bool column_ptrs_populated_ = false;
  std::shared_ptr<types::ColumnWrapper> trace_ids_column_;
  std::shared_ptr<types::ColumnWrapper> stack_traces_column_;
  std::shared_ptr<types::ColumnWrapper> counts_column_;

  uint64_t cumulative_sum_ = 0;
  absl::flat_hash_map<std::string, uint64_t> observed_stack_traces_;
  absl::flat_hash_set<std::string_view> expected_stack_traces_;

  types::ColumnWrapperRecordBatch columns_;

  std::string key2x_;
  std::string key1x_;

  // To reduce variance in results, we add more run-time or add sub-processes:
  static constexpr uint64_t kNumSubProcesses = 4;
  static constexpr std::chrono::seconds kTestRunTime = std::chrono::seconds(120);
};

TEST_F(PerfProfileBPFTest, PerfProfilerGoTest) {
  // Needs to be unique across test fixtures because we use this to map
  // into CPU index. If non-unique, two or more different test fixtures
  // will run their toy apps. on the same CPU.
  constexpr uint32_t kTestIdx = 1;

  const std::filesystem::path bazel_app_path = BazelGoTestAppPath("profiler_test_app_sqrt_go");

  // The toy test app. should be written such that we can expect one stack trace
  // twice as often as another.
  key2x_ = "-;runtime.goexit;runtime.main;main.main;main.sqrtOf1e39;main.sqrt";
  key1x_ = "-;runtime.goexit;runtime.main;main.main;main.sqrtOf1e18;main.sqrt";

  // Populated expected_stack_traces_ with the keys for this test:
  expected_stack_traces_.insert(key2x_);
  expected_stack_traces_.insert(key1x_);

  // Start they toy apps as sub-processes, then,
  // for a certain amount of time (kTestRunTime), collect data using RunTest().
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path, kTestIdx);
  const std::chrono::duration<double> elapsed_time = RunTest();

  // Pull the data into this test (as columns_) using ConsumeRecords(), and
  // find the row indices that belong to our sub-processes using GetTargetRowIdxs().
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
  const std::vector<size_t> target_row_idxs = GetTargetRowIdxs(sub_processes);

  // Populate the cumulative sum & the observed stack traces histo,
  // then check observed vs. expected stack traces key set:
  ASSERT_NO_FATAL_FAILURE(CheckStackTraceIDsInvariance());
  ASSERT_NO_FATAL_FAILURE(PopulateCumulativeSum(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(PopulateObservedStackTraces(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(CheckExpectedVsObservedStackTraces());
  ASSERT_NO_FATAL_FAILURE(CheckExpectedStackTraceCounts(kNumSubProcesses, elapsed_time));
}

TEST_F(PerfProfileBPFTest, PerfProfilerCppTest) {
  // Needs to be unique across test fixtures because we use this to map
  // into CPU index. If non-unique, two or more different test fixtures
  // will run their toy apps. on the same CPU.
  constexpr uint32_t kTestIdx = 0;

  const std::filesystem::path bazel_app_path = BazelCCTestAppPath("profiler_test_app_fib");

  // The toy test app. should be written such that we can expect one stack trace
  // twice as often as another.
  key2x_ = "-;__libc_start_main;main;fib52();fib(unsigned long)";
  key1x_ = "-;__libc_start_main;main;fib27();fib(unsigned long)";

  // Populated expected_stack_traces_ with the keys for this test:
  expected_stack_traces_.insert(key2x_);
  expected_stack_traces_.insert(key1x_);

  // Start they toy apps as sub-processes, then,
  // for a certain amount of time, collect data using RunTest().
  auto sub_processes = StartSubProcesses<CPUPinnedBinaryRunner>(bazel_app_path, kTestIdx);
  const std::chrono::duration<double> elapsed_time = RunTest();

  // Pull the data into this test (as columns_) using ConsumeRecords(), and
  // find the row indices that belong to our sub-processes using GetTargetRowIdxs().
  ASSERT_NO_FATAL_FAILURE(ConsumeRecords());
  const std::vector<size_t> target_row_idxs = GetTargetRowIdxs(sub_processes);

  // Populate the cumulative sum & the observed stack traces histo,
  // then check observed vs. expected stack traces key set:
  ASSERT_NO_FATAL_FAILURE(CheckStackTraceIDsInvariance());
  ASSERT_NO_FATAL_FAILURE(PopulateCumulativeSum(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(PopulateObservedStackTraces(target_row_idxs));
  ASSERT_NO_FATAL_FAILURE(CheckExpectedVsObservedStackTraces());
  ASSERT_NO_FATAL_FAILURE(CheckExpectedStackTraceCounts(kNumSubProcesses, elapsed_time));
}

}  // namespace stirling
}  // namespace pl
