#ifdef __linux__
#include <string.h>
#include <time.h>
#include <algorithm>
#include <thread>
#include <utility>

#include "src/common/utils.h"
#include "src/stirling/bpftrace_connector.h"

#include "third_party/bpftrace/src/ast/codegen_llvm.h"
#include "third_party/bpftrace/src/ast/semantic_analyser.h"
#include "third_party/bpftrace/src/clang_parser.h"
#include "third_party/bpftrace/src/driver.h"
#include "third_party/bpftrace/src/tracepoint_format_parser.h"

namespace pl {
namespace stirling {

// Utility function to convert time as recorded by bpftrace through the 'nsecs' built-in to
// real-time. BPF provides only access to CLOCK_MONOTONIC values (through nsecs), so have to
// determine the offset.
void BPFTraceConnector::InitClockRealTimeOffset() {
  struct timespec time, real_time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  clock_gettime(CLOCK_REALTIME, &real_time);

  real_time_offset_ =
      1000000000ULL * (real_time.tv_sec - time.tv_sec) + real_time.tv_nsec - time.tv_nsec;
}

uint64_t BPFTraceConnector::ClockRealTimeOffset() { return real_time_offset_; }

BPFTraceConnector::BPFTraceConnector(const std::string& source_name, const DataElements& elements,
                                     const char* script, const std::vector<std::string> params)
    : SourceConnector(SourceType::kEBPF, source_name, elements), script_(script), params_(params) {
  // TODO(oazizi): if machine is ever suspended, this would have to be called again.
  InitClockRealTimeOffset();
}

Status BPFTraceConnector::InitImpl() {
  if (!IsRoot()) {
    return error::PermissionDenied("Bpftrace currently only supported as the root user.");
  }

  int err;
  bpftrace::Driver driver;

  // Change these values for debug
  // bpftrace::bt_verbose = true;
  // bpftrace::bt_debug++;

  // Script from file
  // err = driver.parse_file(script_filename_);

  // Script from string (command line argument)
  err = driver.parse_str(script_);
  if (err) {
    return error::Internal("Could not load bpftrace script.");
  }

  // Use this to pass parameters to bpftrace script ($1, $2 in the script)
  for (const auto& param : params_) {
    bpftrace_.add_param(param);
  }

  // Appears to be required for printfs in bt file, so keep them.
  bpftrace_.join_argnum_ = 16;
  bpftrace_.join_argsize_ = 1024;

  err = !bpftrace::TracepointFormatParser::parse(driver.root_);
  if (err) {
    return error::Internal("TracepointFormatParser failed.");
  }

  bpftrace::ClangParser clang;
  clang.parse(driver.root_, bpftrace_.structs_);

  bpftrace::ast::SemanticAnalyser semantics(driver.root_, bpftrace_);
  err = semantics.analyse();
  if (err) {
    return error::Internal("Semantic analyser failed.");
  }

  err = semantics.create_maps(bpftrace::bt_debug != bpftrace::DebugLevel::kNone);
  if (err) {
    return error::Internal("Failed to create BPF maps");
  }

  bpftrace::ast::CodegenLLVM llvm(driver.root_, bpftrace_);
  bpforc_ = llvm.compile(bpftrace::bt_debug);

  if (bpftrace_.num_probes() == 0) {
    return error::Internal("No bpftrace probes to deploy.");
  }

  bool nonblocking_run = true;
  err = bpftrace_.run(bpforc_.get(), nonblocking_run);
  if (err) {
    return error::Internal("Failed to run BPF code.");
  }

  return Status::OK();
}

CPUStatBPFTraceConnector::CPUStatBPFTraceConnector(const std::string& name, uint64_t cpu_id)
    : BPFTraceConnector(name, kElements, kCPUStatBTScript,
                        std::vector<std::string>({std::to_string(cpu_id)})) {
  // Create a data buffer that can hold one record only
  data_buf_.resize(elements_.size());
}

RawDataBuf CPUStatBPFTraceConnector::GetDataImpl() {
  auto cpustat_map = GetBPFMap("@retval");

  for (uint32_t i = 0; i < elements_.size(); ++i) {
    if (elements_[i].type() == DataType::TIME64NS) {
      data_buf_[i] =
          *(reinterpret_cast<int64_t*>(cpustat_map[i].second.data())) + ClockRealTimeOffset();
    } else {
      data_buf_[i] = *(reinterpret_cast<int64_t*>(cpustat_map[i].second.data()));
    }
  }

  return RawDataBuf(1, reinterpret_cast<uint8_t*>(data_buf_.data()));
}

// Helper function for searching through a BPFTraceMap vector of key-value pairs.
// Note that the vector is sorted by keys, and the search is performed sequentially.
// The search will stop as soon as a key >= the search key is found (not just ==).
// This serves two purposes:
// (1) It enables a quicker return.
// (2) It enables resumed searching, when the next search key is >= the previous search key.
// The latter is significant when iteratively comparing elements between two sorted vectors,
// which is the main use case for this function.
// To enable the resumed searching, this function takes the start iterator as an input.
bpftrace::BPFTraceMap::iterator PIDCPUUseBPFTraceConnector::BPFTraceMapSearch(
    const bpftrace::BPFTraceMap& vector, bpftrace::BPFTraceMap::iterator it, uint64_t search_key) {
  auto next_it =
      std::find_if(it, const_cast<bpftrace::BPFTraceMap&>(vector).end(),
                   [&search_key](const std::pair<std::vector<uint8_t>, std::vector<uint8_t>>& x) {
                     return *(reinterpret_cast<const uint32_t*>(x.first.data())) >= search_key;
                   });
  return next_it;
}

RawDataBuf PIDCPUUseBPFTraceConnector::GetDataImpl() {
  auto pid_time_pairs = GetBPFMap("@total_time");
  auto num_pids = pid_time_pairs.size();

  pid_name_pairs_ = GetBPFMap("@names");

  // This is a special map with only one entry at location 0.
  auto sampling_time = GetBPFMap("@time");
  CHECK_EQ(1ULL, sampling_time.size());
  auto timestamp = *(reinterpret_cast<int64_t*>(sampling_time[0].second.data()));

  // TODO(oazizi): Optimize this. Likely need a way of removing old PIDs in the bt file.
  data_buf_.resize(std::max<uint64_t>(num_pids * (elements_.size()), data_buf_.size()));

  auto last_result_it = last_result_times_.begin();
  auto pid_name_it = pid_name_pairs_.begin();

  RawDataBuf raw_data_buf(num_pids, reinterpret_cast<uint8_t*>(data_buf_.data()));

  uint32_t idx = 0;
  for (auto& pid_time_pair : pid_time_pairs) {
    auto key = pid_time_pair.first;
    auto value = pid_time_pair.second;

    uint64_t cputime = *(reinterpret_cast<uint64_t*>(value.data()));

    DCHECK_EQ(4ULL, key.size()) << "Expected uint32_t key";
    uint64_t pid = *(reinterpret_cast<uint32_t*>(key.data()));

    // Get the name from the auxiliary BPFTraceMap for names.
    char* name = nullptr;
    pid_name_it = BPFTraceMapSearch(pid_name_pairs_, pid_name_it, pid);
    if (pid_name_it != pid_name_pairs_.end()) {
      uint32_t found_pid = *(reinterpret_cast<uint32_t*>(pid_name_it->first.data()));
      if (found_pid == pid) {
        name = reinterpret_cast<char*>(pid_name_it->second.data());
      } else {
        // Couldn't find the name for the PID.
        // May happen in practice due to a race with how the kernel populates the BPF maps.
        LOG(WARNING) << absl::StrFormat("Could not find a name for the PID %d", pid);
      }
    }

    // Get the last cpu time from the BPFTraceMap from previous call to this function.
    uint64_t last_cputime = 0;
    last_result_it = BPFTraceMapSearch(last_result_times_, last_result_it, pid);
    if (last_result_it != last_result_times_.end()) {
      uint32_t found_pid = *(reinterpret_cast<uint32_t*>(last_result_it->first.data()));
      if (found_pid == pid) {
        last_cputime = *(reinterpret_cast<uint64_t*>(last_result_it->second.data()));
      }
    }

    data_buf_[idx++] = timestamp + ClockRealTimeOffset();
    data_buf_[idx++] = pid;
    data_buf_[idx++] = cputime - last_cputime;
    data_buf_[idx++] = reinterpret_cast<uint64_t>(name);
  }

  // Keep this, because we will want to compute deltas next time.
  last_result_times_ = std::move(pid_time_pairs);

  return RawDataBuf(num_pids, reinterpret_cast<uint8_t*>(data_buf_.data()));
}

Status BPFTraceConnector::StopImpl() {
  // TODO(oazizi): Test this. Not guaranteed to work.
  bpftrace_.stop();
  return Status::OK();
}

}  // namespace stirling
}  // namespace pl

#endif
