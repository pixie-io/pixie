#ifdef __linux__
#include <algorithm>
#include <cstring>
#include <ctime>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/bpftrace_connector.h"

#include "third_party/bpftrace/src/ast/codegen_llvm.h"
#include "third_party/bpftrace/src/ast/semantic_analyser.h"
#include "third_party/bpftrace/src/clang_parser.h"
#include "third_party/bpftrace/src/driver.h"
#include "third_party/bpftrace/src/tracepoint_format_parser.h"

namespace pl {
namespace stirling {

BPFTraceConnector::BPFTraceConnector(std::string_view source_name,
                                     const ConstVectorView<DataTableSchema>& schemas,
                                     std::chrono::milliseconds default_sampling_period,
                                     std::chrono::milliseconds default_push_period,
                                     const std::string_view script, std::vector<std::string> params)
    : SourceConnector(SourceType::kEBPF, source_name, schemas, default_sampling_period,
                      default_push_period),
      script_(script),
      params_(std::move(params)) {
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
  err = driver.parse_str(std::string(script_));
  if (err != 0) {
    return error::Internal("Could not load bpftrace script.");
  }

  // Use this to pass parameters to bpftrace script ($1, $2 in the script)
  for (const auto& param : params_) {
    bpftrace_.add_param(param);
  }

  // Appears to be required for printfs in bt file, so keep them.
  bpftrace_.join_argnum_ = 16;
  bpftrace_.join_argsize_ = 1024;

  err = static_cast<int>(!bpftrace::TracepointFormatParser::parse(driver.root_));
  if (err != 0) {
    return error::Internal("TracepointFormatParser failed.");
  }

  bpftrace::ClangParser clang;
  clang.parse(driver.root_, bpftrace_.structs_);

  bpftrace::ast::SemanticAnalyser semantics(driver.root_, bpftrace_);
  err = semantics.analyse();
  if (err != 0) {
    return error::Internal("Semantic analyser failed.");
  }

  err = semantics.create_maps(bpftrace::bt_debug != bpftrace::DebugLevel::kNone);
  if (err != 0) {
    return error::Internal("Failed to create BPF maps");
  }

  bpftrace::ast::CodegenLLVM llvm(driver.root_, bpftrace_);
  bpforc_ = llvm.compile(bpftrace::bt_debug);

  if (bpftrace_.num_probes() == 0) {
    return error::Internal("No bpftrace probes to deploy.");
  }

  bool nonblocking_run = true;
  err = bpftrace_.run(bpforc_.get(), nonblocking_run);
  if (err != 0) {
    return error::Internal("Failed to run BPF code.");
  }

  return Status::OK();
}

CPUStatBPFTraceConnector::CPUStatBPFTraceConnector(std::string_view name, uint64_t cpu_id)
    : BPFTraceConnector(name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod, kBTScript,
                        std::vector<std::string>({std::to_string(cpu_id)})) {}

void CPUStatBPFTraceConnector::TransferDataImpl(uint32_t table_num,
                                                types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_LT(table_num, kTables.size())
      << absl::StrFormat("Trying to access unexpected table: table_num=%d", table_num);

  auto& columns = *record_batch;

  auto cpustat_map = GetBPFMap("@retval");

  auto data_elements = elements(table_num);

  // If kernel hasn't populated BPF map yet, then we have no data to return.
  if (cpustat_map.size() != data_elements.size()) {
    return;
  }

  for (uint32_t i = 0; i < data_elements.size(); ++i) {
    if (data_elements[i].type() == types::DataType::TIME64NS) {
      types::Time64NSValue val =
          *(reinterpret_cast<int64_t*>(cpustat_map[i].second.data())) + ClockRealTimeOffset();
      columns[i]->Append(val);
    } else {
      types::Int64Value val = *(reinterpret_cast<int64_t*>(cpustat_map[i].second.data()));
      columns[i]->Append(val);
    }
  }
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

PIDCPUUseBPFTraceConnector::PIDCPUUseBPFTraceConnector(std::string_view name)
    : BPFTraceConnector(name, kTables, kDefaultSamplingPeriod, kDefaultPushPeriod, kBTScript,
                        std::vector<std::string>({})) {}

void PIDCPUUseBPFTraceConnector::TransferDataImpl(uint32_t table_num,
                                                  types::ColumnWrapperRecordBatch* record_batch) {
  CHECK_LT(table_num, kTables.size())
      << absl::StrFormat("Trying to access unexpected table: table_num=%d", table_num);

  auto& columns = *record_batch;

  auto pid_time_pairs = GetBPFMap("@total_time");
  auto pid_name_pairs = GetBPFMap("@names");

  // This is a special map with only one entry at location 0.
  auto sampling_time = GetBPFMap("@time");
  CHECK_EQ(1ULL, sampling_time.size());
  auto timestamp = *(reinterpret_cast<int64_t*>(sampling_time[0].second.data()));

  auto last_result_it = last_result_times_.begin();
  auto pid_name_it = pid_name_pairs.begin();

  for (auto& pid_time_pair : pid_time_pairs) {
    auto key = pid_time_pair.first;
    auto value = pid_time_pair.second;

    uint64_t cputime = *(reinterpret_cast<uint64_t*>(value.data()));

    DCHECK_EQ(4ULL, key.size()) << "Expected uint32_t key";
    uint64_t pid = *(reinterpret_cast<uint32_t*>(key.data()));

    // Get the name from the auxiliary BPFTraceMap for names.
    std::string name("-");
    pid_name_it = BPFTraceMapSearch(pid_name_pairs, pid_name_it, pid);
    if (pid_name_it != pid_name_pairs.end()) {
      uint32_t found_pid = *(reinterpret_cast<uint32_t*>(pid_name_it->first.data()));
      if (found_pid == pid) {
        name = std::string(reinterpret_cast<char*>(pid_name_it->second.data()));
      } else {
        // Couldn't find the name for the PID.
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

    uint32_t idx = 0;
    columns[idx++]->Append<types::Time64NSValue>(timestamp + ClockRealTimeOffset());
    columns[idx++]->Append<types::Int64Value>(pid);
    columns[idx++]->Append<types::Int64Value>(cputime - last_cputime);
    columns[idx++]->Append<types::StringValue>(std::move(name));
  }

  // Keep this, because we will want to compute deltas next time.
  last_result_times_ = std::move(pid_time_pairs);
}

Status BPFTraceConnector::StopImpl() {
  // TODO(oazizi): Test this. Not guaranteed to work.
  bpftrace_.stop();
  return Status::OK();
}

}  // namespace stirling
}  // namespace pl

#endif
