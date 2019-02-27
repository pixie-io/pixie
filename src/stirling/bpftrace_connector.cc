#ifdef __linux__
#include <time.h>
#include <thread>

#include "src/common/utils.h"
#include "src/stirling/bpftrace_connector.h"

#include "third_party/bpftrace/src/ast/codegen_llvm.h"
#include "third_party/bpftrace/src/ast/semantic_analyser.h"
#include "third_party/bpftrace/src/clang_parser.h"
#include "third_party/bpftrace/src/driver.h"

namespace pl {
namespace stirling {

// Utility function to convert time as recorded by bpftrace through the 'nsecs' built-in to
// real-time. BPF provides only access to CLOCK_MONOTONIC values (through nsecs), so have to
// determine the offset.
uint64_t BPFTraceConnector::ClockRealTimeOffset() {
  struct timespec time, real_time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  clock_gettime(CLOCK_REALTIME, &real_time);

  auto offset = 1000000000ULL * (real_time.tv_sec - time.tv_sec) + real_time.tv_nsec - time.tv_nsec;

  return offset;
}

BPFTraceConnector::BPFTraceConnector(const std::string& source_name, const DataElements& elements,
                                     const char* script, const std::vector<std::string> params)
    : SourceConnector(SourceType::kEBPF, source_name, elements), script_(script), params_(params) {
  // Create a data buffer that can hold one record only
  data_buf_.resize(sizeof(uint64_t) * elements_.size());

  // TODO(oazizi): if machine is ever suspended, this would have to be called again.
  real_time_offset_ = ClockRealTimeOffset();
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

  err = bpftrace_.run(bpforc_.get());
  if (err) {
    return error::Internal("Failed to run BPF code.");
  }

  return Status::OK();
}

RawDataBuf BPFTraceConnector::GetDataImpl() {
  auto result_maps = bpftrace_.return_maps();

  uint64_t* data_buf_ptr = reinterpret_cast<uint64_t*>(data_buf_.data());

  // TODO(oazizi): Support multiple return maps.
  CHECK_EQ(1ULL, result_maps.size()) << "Only one map is supported";

  // TODO(oazizi): Map is assumed to be an simple array. Add support for other maps.

  for (auto result_map : result_maps) {
    auto value_map = result_map.second;

    for (uint32_t i = 0; i < elements_.size(); ++i) {
      if (elements_[i].type() == DataType::TIME64NS) {
        data_buf_ptr[i] = value_map[i] + real_time_offset_;
      } else {
        data_buf_ptr[i] = value_map[i];
      }
    }
  }

  return RawDataBuf(1, data_buf_.data());
}

Status BPFTraceConnector::StopImpl() {
  // TODO(oazizi): Test this. Not guaranteed to work.
  bpftrace_.stop();
  return Status::OK();
}

}  // namespace stirling
}  // namespace pl

#endif
