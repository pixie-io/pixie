#ifdef __linux__
#include <time.h>
#include <algorithm>
#include <thread>

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
  auto result_maps = GetResultMaps();
  auto cpustat_map = result_maps["@retval"];

  for (uint32_t i = 0; i < elements_.size(); ++i) {
    if (elements_[i].type() == DataType::TIME64NS) {
      data_buf_[i] = cpustat_map[i] + ClockRealTimeOffset();
    } else {
      data_buf_[i] = cpustat_map[i];
    }
  }

  return RawDataBuf(1, reinterpret_cast<uint8_t*>(data_buf_.data()));
}

RawDataBuf PIDCPUUseBPFTraceConnector::GetDataImpl() {
  auto result_maps = GetResultMaps();

  auto pid_to_time_map = result_maps["@total_time"];
  auto num_pids = pid_to_time_map.size();

  // TODO(oazizi): Get PID process names, like below.
  // auto pid_to_name_map = result_maps["@names"];

  // This is a special map with only one entry at location 0.
  auto sampling_time_map = result_maps["@time"];
  CHECK_EQ(1ULL, sampling_time_map.size());
  auto timestamp = sampling_time_map[0];

  // TODO(oazizi): Optimize this. Likely need a way of removing old PIDs in the bt file.
  data_buf_.resize(std::max<uint64_t>(num_pids * elements_.size(), data_buf_.size()));

  uint32_t idx = 0;
  for (auto pid_time_pair : pid_to_time_map) {
    auto pid = pid_time_pair.first;
    auto cputime = pid_time_pair.second;

    data_buf_[idx++] = timestamp + ClockRealTimeOffset();
    data_buf_[idx++] = pid;
    data_buf_[idx++] = cputime - last_result_[pid];
  }

  last_result_ = pid_to_time_map;

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
