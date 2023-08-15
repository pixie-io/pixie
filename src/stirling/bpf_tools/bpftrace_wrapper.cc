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

#include "src/stirling/bpf_tools/bpftrace_wrapper.h"

#include <aot/aot.h>
#include <ast/pass_manager.h>
#include <ast/passes/codegen_llvm.h>
#include <ast/passes/field_analyser.h>
#include <ast/passes/node_counter.h>
#include <ast/passes/printer.h>
#include <ast/passes/resource_analyser.h>
#include <ast/passes/semantic_analyser.h>
#include <bpftrace.h>
#include <clang_parser.h>
#include <driver.h>
#include <tracepoint_format_parser.h>

#include <limits>
#include <sstream>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/system/config.h"
#include "src/stirling/utils/linux_headers.h"

namespace px {
namespace stirling {
namespace bpf_tools {

using ::bpftrace::Driver;
using ::bpftrace::ast::Printer;

std::string DumpDriver(const Driver& driver) {
  std::ostringstream oss;

  Printer p(oss);
  driver.root->accept(p);
  return oss.str();
}

// Required to support strftime() in bpftrace code.
// Since BPF nsecs uses monotonic clock, but strftime() needs to know the real time,
// BPFtrace requires the offset to be passed in directly.
// BPFTrace then applies the offset before performing the formatting.
struct timespec GetBootTime() {
  constexpr uint64_t kNanosPerSecond = 1000 * 1000 * 1000;

  // Convert the current monotonic time to real time and calculate an offset. With our current real
  // time conversion this is equivalent, but when we add more complicated time conversion this won't
  // be 100% accurate for the duration of the probe. But since its only used for strftime in
  // BPFTrace it won't matter for our BPFTrace scripts.
  struct timespec mono_time;
  clock_gettime(CLOCK_MONOTONIC, &mono_time);
  uint64_t mono_nsecs = kNanosPerSecond * mono_time.tv_sec + mono_time.tv_nsec;
  uint64_t real_nsecs = px::system::Config::GetInstance().ConvertToRealTime(mono_nsecs);
  uint64_t time_offset = real_nsecs - mono_nsecs;

  struct timespec boottime;
  boottime.tv_sec = time_offset / kNanosPerSecond;
  boottime.tv_nsec = time_offset % kNanosPerSecond;

  return boottime;
}

BPFTraceWrapper::BPFTraceWrapper() {
  bpftrace_.ast_max_nodes_ = std::numeric_limits<uint64_t>::max();
  bpftrace_.boottime_ = GetBootTime();

  // Change these values for debug
  // bpftrace::bt_verbose = true;
  // bpftrace::bt_debug = bpftrace::DebugLevel::kFullDebug;

  // Suppress bpftrace output to avoid pollution.
  bpftrace::bt_quiet = true;
}

Status BPFTraceWrapper::CompileForPrintfOutput(std::string_view script,
                                               const std::vector<std::string>& params) {
  PX_RETURN_IF_ERROR(Compile(script, params));
  PX_RETURN_IF_ERROR(CheckPrintfs());
  printf_to_table_ = true;
  return Status::OK();
}

Status BPFTraceWrapper::CompileForMapOutput(std::string_view script,
                                            const std::vector<std::string>& params) {
  PX_RETURN_IF_ERROR(Compile(script, params));
  return Status::OK();
}

std::vector<std::string> ClangCompileFlags(bool has_btf, std::vector<std::string> include_dirs = {},
                                           std::vector<std::string> include_files = {}) {
  std::vector<std::string> extra_flags;
  {
    struct utsname utsname;
    uname(&utsname);
    std::string ksrc, kobj;
    auto kdirs = bpftrace::get_kernel_dirs(utsname, !has_btf);
    ksrc = std::get<0>(kdirs);
    kobj = std::get<1>(kdirs);

    if (ksrc != "") {
      extra_flags = bpftrace::get_kernel_cflags(utsname.machine, ksrc, kobj);
    }
  }
  extra_flags.push_back("-include");
  extra_flags.push_back(CLANG_WORKAROUNDS_H);

  for (auto dir : include_dirs) {
    extra_flags.push_back("-I");
    extra_flags.push_back(dir);
  }
  for (auto file : include_files) {
    extra_flags.push_back("-include");
    extra_flags.push_back(file);
  }

  return extra_flags;
}

// Redirects stderr and collect the output to a string.
class CerrRedirect {
 public:
  CerrRedirect() { old_ = std::cerr.rdbuf(buffer_.rdbuf()); }
  ~CerrRedirect() { std::cerr.rdbuf(old_); }
  std::string GetString() const { return buffer_.str(); }

 private:
  std::stringstream buffer_;
  std::streambuf* old_;
};

// This compile function is inspired from the bpftrace project's main.cpp.
// Changes to bpftrace may need to be reflected back to this function on a bpftrace update.
Status BPFTraceWrapper::Compile(std::string_view script, const std::vector<std::string>& params) {
  // Because BPFTrace uses global state (related to clear_struct_list()),
  // multiple simultaneous compiles may not be safe. For now, introduce a lock for safety.
  // TODO(oazizi): Update BPFTrace repo to avoid use of global state if possible.

  const std::lock_guard<std::mutex> lock(compilation_mutex_);

  // Some functions below return errors, while others success as positive numbers.
  // For readability, use two separate variables for the two models.
  int err;
  int success;
  std::string err_msg;

  // This ensures system headers be installed correctly inside a container.
  PX_RETURN_IF_ERROR(utils::FindOrInstallLinuxHeaders());

  // Reset some BPFTrace global state, which may be dirty because of a previous compile.
  bpftrace::TracepointFormatParser::clear_struct_list();

  // Use this to pass parameters to bpftrace script ($1, $2 in the script)
  for (const auto& param : params) {
    bpftrace_.add_param(param);
  }

#define ERR_MSG "Could not compile bpftrace script, "

  // Script from string (command line argument)
  bpftrace::Driver driver(bpftrace_);
  driver.source(/*file_name*/ "stdin", std::string(script));
  {
    CerrRedirect cerr_redirect;
    // Driver calls yacc's generated parser, which has a very complicated error reporting structure.
    // So capture std::cerr for collecting errors.
    err = driver.parse();
    err_msg = cerr_redirect.GetString();
  }
  if (err != 0) {
    return error::Internal(ERR_MSG "failed to parse: $0", err_msg);
  }

  bpftrace_.parse_btf(driver.list_modules());

  // This collects the error messages emitted during the FieldAnalyser's analysis.
  std::ostringstream field_analyser_oss;
  bpftrace::ast::FieldAnalyser fields(driver.root.get(), bpftrace_, field_analyser_oss);
  err = fields.analyse();
  if (err != 0) {
    return error::Internal(ERR_MSG "invalid field: $0", field_analyser_oss.str());
  }

  {
    CerrRedirect cerr_redirect;
    // TracepointFormatParser does not provide internal error recording, and the error is directly
    // logged to std::cerr.
    success = bpftrace::TracepointFormatParser::parse(driver.root.get(), bpftrace_);
    err_msg = cerr_redirect.GetString();
  }
  if (!success) {
    return error::Internal(ERR_MSG "invalid tracepoint: $0", err_msg);
  }

  std::vector<std::string> clang_compile_flags = ClangCompileFlags(bpftrace_.feature_->has_btf());

  bpftrace::ClangParser clang;
  // TODO(yzhao): Return error messages from ClangParserHandler::get_error_messages().
  // Needs to update bpftrace source code.
  std::vector<std::string> clang_parser_err_msgs;
  success = clang.parse(driver.root.get(), bpftrace_, clang_compile_flags, &clang_parser_err_msgs);
  if (!success) {
    return error::Internal(ERR_MSG "Clang parse failed: $0",
                           absl::StrJoin(clang_parser_err_msgs, "\n"));
  }

  bpftrace::ast::PassContext ctx(bpftrace_);
  bpftrace::ast::PassManager pm;
  pm.AddPass(bpftrace::ast::CreateSemanticPass());
  pm.AddPass(bpftrace::ast::CreateCounterPass());
  pm.AddPass(bpftrace::ast::CreateResourcePass());
  auto result = pm.Run(std::move(driver.root), ctx);
  if (!result.Ok()) {
    return error::Internal(ERR_MSG "$0 pass failed: $1",
                           result.GetErrorPass().value_or("Unknown pass"),
                           result.GetErrorMsg().value_or("-"));
  }
  std::unique_ptr<bpftrace::ast::Node> ast_root(result.Root());

  bpftrace::ast::CodegenLLVM llvm(ast_root.get(), bpftrace_);
  llvm.generate_ir();
  llvm.optimize();
  bytecode_ = llvm.emit();

  compiled_ = true;

#undef ERR_MSG
  return Status::OK();
}

Status BPFTraceWrapper::Deploy(const PrintfCallback& printf_callback) {
  DCHECK(compiled_) << "Must compile first.";
  DCHECK_EQ(printf_callback != nullptr, printf_to_table_)
      << "Provide callback if and only if compiled for printfs output";

  if (bpftrace_.num_probes() == 0) {
    return error::Internal("No bpftrace probes to deploy.");
  }

  if (!IsRoot()) {
    return error::PermissionDenied("Bpftrace currently only supported as the root user.");
  }

  if (printf_callback) {
    bpftrace_.printf_callback_ = printf_callback;
  }

  int err = bpftrace_.deploy(bytecode_);
  if (err != 0) {
    return error::Internal("Failed to run BPF code.");
  }
  return Status::OK();
}

void BPFTraceWrapper::PollPerfBuffers(int timeout_ms) {
  bpftrace_.poll_perf_events(/* drain */ false, timeout_ms);
}

void BPFTraceWrapper::Stop() {
  // There is no need to manually cleanup bpftrace_.
}

Status BPFTraceWrapper::CheckPrintfs() const {
  if (bpftrace_.resources.printf_args.empty()) {
    return error::Internal("The BPFTrace program must contain at least one printf statement.");
  }

  const auto& fmt = std::get<0>(bpftrace_.resources.printf_args[0]);
  for (size_t i = 1; i < bpftrace_.resources.printf_args.size(); ++i) {
    const auto& fmt_i = std::get<0>(bpftrace_.resources.printf_args[i]);
    if (fmt_i.str() != fmt.str()) {
      return error::Internal(
          "All printf statements must have exactly the same format string. [$0] does not match "
          "[$1]",
          fmt_i.str(), fmt.str());
    }
  }

  return Status::OK();
}

const std::vector<bpftrace::Field>& BPFTraceWrapper::OutputFields() const {
  DCHECK(compiled_) << "Must compile first.";
  DCHECK(printf_to_table_) << "OutputFields() on supported if compiling with printf_to_table";

  return std::get<1>(bpftrace_.resources.printf_args.front());
}

std::string_view BPFTraceWrapper::OutputFmtStr() const {
  DCHECK(compiled_) << "Must compile first.";
  DCHECK(printf_to_table_) << "OutputFmtStr() on supported if compiling with printf_to_table";

  const auto& fmt_str = std::get<0>(bpftrace_.resources.printf_args.front());
  return std::string_view(fmt_str.c_str(), fmt_str.length());
}

bpftrace::BPFTraceMap BPFTraceWrapper::GetBPFMap(const std::string& name) {
  return bpftrace_.get_map(name);
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
