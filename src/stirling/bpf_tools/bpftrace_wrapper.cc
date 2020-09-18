#ifdef __linux__

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

#include "src/common/base/base.h"

#include "third_party/bpftrace/src/ast/codegen_llvm.h"
#include "third_party/bpftrace/src/ast/semantic_analyser.h"
#include "third_party/bpftrace/src/clang_parser.h"
#include "third_party/bpftrace/src/driver.h"
#include "third_party/bpftrace/src/procmon.h"
#include "third_party/bpftrace/src/tracepoint_format_parser.h"

namespace pl {
namespace stirling {
namespace bpf_tools {

Status BPFTraceWrapper::Deploy(
    std::string_view script, const std::vector<std::string>& params,
    const std::function<void(const std::vector<bpftrace::Field>, uint8_t*)>& printf_callback) {
  if (!IsRoot()) {
    return error::PermissionDenied("Bpftrace currently only supported as the root user.");
  }

  int err;
  bpftrace::Driver driver(bpftrace_);

  // Change these values for debug
  // bpftrace::bt_verbose = true;
  // bpftrace::bt_debug++;

  // Script from string (command line argument)
  err = driver.parse_str(std::string(script));
  if (err != 0) {
    return error::Internal("Could not load bpftrace script.");
  }

  // Use this to pass parameters to bpftrace script ($1, $2 in the script)
  for (const auto& param : params) {
    bpftrace_.add_param(param);
  }

  // Appears to be required for printfs in bt file, so keep them.
  bpftrace_.join_argnum_ = 16;
  bpftrace_.join_argsize_ = 1024;

  err = static_cast<int>(!bpftrace::TracepointFormatParser::parse(driver.root_.get(), bpftrace_));
  if (err != 0) {
    return error::Internal("TracepointFormatParser failed.");
  }

  bpftrace::ClangParser clang;
  clang.parse(driver.root_.get(), bpftrace_);

  bpftrace::ast::SemanticAnalyser semantics(driver.root_.get(), bpftrace_, bpftrace_.feature_);
  err = semantics.analyse();
  if (err != 0) {
    return error::Internal("Semantic analyser failed.");
  }

  err = semantics.create_maps(bpftrace::bt_debug != bpftrace::DebugLevel::kNone);
  if (err != 0) {
    return error::Internal("Failed to create BPF maps");
  }

  bpftrace::ast::CodegenLLVM llvm(driver.root_.get(), bpftrace_);
  bpforc_ = llvm.compile();

  if (bpftrace_.num_probes() == 0) {
    return error::Internal("No bpftrace probes to deploy.");
  }

  if (printf_callback) {
    bpftrace_.printf_callback_ = printf_callback;
  }
  bpftrace_.bpforc_ = bpforc_.get();
  err = bpftrace_.deploy();
  if (err != 0) {
    return error::Internal("Failed to run BPF code.");
  }
  return Status::OK();
}

void BPFTraceWrapper::PollPerfBuffers(int timeout_ms) {
  bpftrace_.poll_perf_events(/* drain */ false, timeout_ms);
}

void BPFTraceWrapper::Stop() { bpftrace_.finalize(); }

StatusOr<std::vector<bpftrace::Field>> BPFTraceWrapper::OutputFields() {
  if (bpftrace_.printf_args_.size() != 1) {
    return error::Internal(
        "The BPFTrace program must contain exactly one printf statement, but found $0.",
        bpftrace_.printf_args_.size());
  }

  return std::get<1>(bpftrace_.printf_args_.front());
}

bpftrace::BPFTraceMap BPFTraceWrapper::GetBPFMap(const std::string& name) {
  return bpftrace_.get_map(name);
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace pl

#endif
