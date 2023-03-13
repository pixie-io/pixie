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

#include <string>

#include <absl/functional/bind_front.h>
#include <prometheus/counter.h>

#include "src/common/fs/fs_wrapper.h"
#include "src/common/metrics/metrics.h"
#include "src/common/system/proc_parser.h"
#include "src/common/system/proc_pid_path.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/raw_symbol_update.h"
#include "src/stirling/source_connectors/perf_profiler/java/demangle.h"
#include "src/stirling/source_connectors/perf_profiler/shared/symbolization.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/java_symbolizer.h"
#include "src/stirling/utils/detect_application.h"
#include "src/stirling/utils/proc_tracker.h"

namespace {
char const* const kLibsHelpMessage = "Comma separated list of Java symbolization agent lib files.";
char const* const kPEMAgentLibs = "/px/libpx-java-agent.so";
}  // namespace

DEFINE_string(stirling_profiler_java_agent_libs, kPEMAgentLibs, kLibsHelpMessage);
DECLARE_bool(stirling_profiler_java_symbols);
DEFINE_uint32(number_attach_attempts_per_iteration, 1,
              "Number of JVMTI agents that can be attached in a single perf profiler iteration.");

namespace px {
namespace stirling {

namespace {

constexpr char kJavaProcCounter[] = "java_proc";
constexpr char kJavaProcAttachAttemptedCounter[] = "java_proc_attach_attempted";
constexpr char kJavaProcAttachedCounter[] = "java_proc_attached";

prometheus::Counter& g_java_proc_counter{
    BuildCounter(kJavaProcCounter, "Count of the Java processes")};

prometheus::Counter& g_java_proc_attach_attempted_counter{
    BuildCounter(kJavaProcAttachAttemptedCounter,
                 "Count of the Java processes that had been attempted to attach profiling agent")};

prometheus::Counter& g_java_proc_attached_counter{BuildCounter(
    kJavaProcAttachedCounter, "Count of the Java processes that has profiling agent attached")};

Status SpaceAvailableForAgentLibsAndSymbolFile(const struct upid_t& upid) {
  const std::filesystem::path tmp_path = java::StirlingTmpPathForUPID(upid);
  auto status_or_space_available = fs::SpaceAvailableInBytes(tmp_path);
  if (!status_or_space_available.ok()) {
    // Could not figure out how much storage capacity is available in tmp path for the target pid.
    // Return the status. The symbolization agent attach process will abort.
    const system::ProcParser proc_parser;
    const std::string cmdline = proc_parser.GetPIDCmdline(upid.pid);
    const std::string error_msg = absl::Substitute(
        "Could not determine space available in path $0, for Java pid: $1, cmd: $2.",
        tmp_path.string(), upid.pid, cmdline);
    LOG(WARNING) << error_msg;
    return error::Internal("$0: $1", error_msg, status_or_space_available.msg());
  }
  const int64_t space_available_in_bytes = status_or_space_available.ConsumeValueOrDie();
  constexpr int64_t kMinimumBytesRequired = 16 * 1024 * 1024;
  if (space_available_in_bytes < kMinimumBytesRequired) {
    // Available capacity in "tmp" for the target process is less than the minimum required.
    // Return an error to indicate "no space available." The agent attach process will abort.
    const system::ProcParser proc_parser;
    const std::string cmdline = proc_parser.GetPIDCmdline(upid.pid);
    const std::string error_msg = absl::Substitute(
        "Not enough tmp space available for Java symbolization libraries and symbol file. Found $0 "
        "bytes available in $1 (but require $2 bytes), for Java pid: $3, cmd: $4.",
        space_available_in_bytes, tmp_path.string(), kMinimumBytesRequired, upid.pid, cmdline);
    LOG(WARNING) << error_msg;
    return error::Internal(error_msg);
  }
  return Status::OK();
}
}  // namespace

void JavaSymbolizationContext::RemoveArtifacts() const {
  // Remove the host artifacts path entirely; this cleans up all the files (and the subdir) we
  // created inside of the target container mount namespace.
  const auto& sysconfig = system::Config::GetInstance();
  const std::filesystem::path host_artifacts_path = sysconfig.ToHostPath(host_artifacts_path_);
  if (fs::Exists(host_artifacts_path)) {
    const Status s = fs::RemoveAll(host_artifacts_path);
    char const* const warn = "Could not remove host artifacts path: $0, $1.";
    LOG_IF(WARNING, !s.ok()) << absl::Substitute(warn, host_artifacts_path_.string(), s.msg());
  }
}

void JavaSymbolizationContext::UpdateSymbolMap() {
  auto reset_symbol_file = [&](const auto pos) {
    symbol_file_->seekg(pos);
    symbol_file_->clear();
    DCHECK(symbol_file_->good());
  };

  java::RawSymbolUpdate update;

  std::string buffer;
  std::string symbol;
  std::string fn_sig;
  std::string class_sig;

  buffer.reserve(300);
  symbol.reserve(100);
  fn_sig.reserve(100);
  class_sig.reserve(100);

  while (true) {
    const auto pos = symbol_file_->tellg();

    symbol_file_->read(reinterpret_cast<char*>(&update), sizeof(java::RawSymbolUpdate));
    if (!symbol_file_->good()) {
      // No data to be read. Break from the loop. Pedantically reset file pos so that when we
      // return here, we are in the correct state.
      reset_symbol_file(pos);
      break;
    }

    const uint64_t n = update.TotalNumSymbolBytes();

    if (buffer.capacity() < n) {
      buffer.resize(n);
    }

    symbol_file_->read(buffer.data(), n);
    if (!symbol_file_->good()) {
      // If the read fails, then the symbol file was left in a partially written state.
      // Reset the file position back to the beginning of a symbol, and break out of this loop.
      reset_symbol_file(pos);
      break;
    }

    // At this point, we have consumed an entire udpate from the symbol file.
    // We either put a new symbol into the symbol map (common case) or remove a symbol.

    if (update.method_unload) {
      // Handle remove symbol scenario.
      // NB: if we go back to caching Java symbols, we will need to invalidate
      // any cached instances of this symbol.
      symbol_map_.erase(update.addr);
      continue;
    }

    // TODO(jps): Make the interface to the demangler consume string_view only, then
    // convert symbol, fn_sig, and class_sig to string_view (reduces copying).
    // TODO(jps): Remove null terminating character from java::RawSymbolUpdate.
    symbol.assign(buffer.data() + update.SymbolOffset(), update.symbol_size - 1);
    fn_sig.assign(buffer.data() + update.FnSigOffset(), update.fn_sig_size - 1);
    class_sig.assign(buffer.data() + update.ClassSigOffset(), update.class_sig_size - 1);

    using symbolization::kJavaPrefix;
    const auto demangled = absl::StrCat(kJavaPrefix, java::Demangle(symbol, class_sig, fn_sig));

    // TODO(jps): Change to uint32_t in java::RawSymbolUpdate.
    const uint32_t code_size = static_cast<uint32_t>(update.code_size);
    symbol_map_.try_emplace(update.addr, demangled, code_size);
  }
  DCHECK(symbol_file_->good());
}

JavaSymbolizationContext::JavaSymbolizationContext(const struct upid_t& target_upid,
                                                   profiler::SymbolizerFn native_symbolizer_fn,
                                                   std::unique_ptr<std::ifstream> symbol_file)
    : native_symbolizer_fn_(native_symbolizer_fn), symbol_file_(std::move(symbol_file)) {
  DCHECK(symbol_file_->good());
  UpdateSymbolMap();

  host_artifacts_path_ = java::AgentArtifactsPath(target_upid);
}

JavaSymbolizationContext::~JavaSymbolizationContext() { symbol_file_->close(); }

std::string_view JavaSymbolizationContext::Symbolize(const uintptr_t addr) {
  if (requires_refresh_) {
    // Member requires_refresh_ is set by IterationPreTick(), which is called "once per iteration,"
    // i.e. is set to true each time we drain the stack trace data from the underlying BPF
    // data tables. We will only attempt to update the symbol map (and possibly incur expensive
    // syscalls for file IO) if this member is set and we are symbolizing in this context.
    // Subsequent calls to Symbolize() in this iteration will not attempt to update the symbol map.
    UpdateSymbolMap();
    requires_refresh_ = false;
  }

  static std::string symbol;

  if (symbol_map_.size() > 0) {
    auto it = symbol_map_.upper_bound(addr);
    if (it != symbol_map_.begin()) {
      it--;
      const uint64_t addr_lower = it->first;
      const uint64_t addr_upper = addr_lower + it->second.size;
      if ((addr_lower <= addr) && (addr < addr_upper)) {
        symbol = it->second.symbol;
        return symbol;
      }
    }
  }
  return native_symbolizer_fn_(addr);
}

JavaSymbolizer::JavaSymbolizer(std::string&& agent_libs) : agent_libs_(std::move(agent_libs)) {}

StatusOr<std::unique_ptr<Symbolizer>> JavaSymbolizer::Create(
    std::unique_ptr<Symbolizer> native_symbolizer) {
  const std::string& comma_separated_libs = FLAGS_stirling_profiler_java_agent_libs;
  const std::vector<std::string_view> lib_args = absl::StrSplit(comma_separated_libs, ",");
  std::vector<std::string> abs_path_libs;
  for (const auto& lib : lib_args) {
    PX_ASSIGN_OR(const auto abs_path_lib, fs::Absolute(lib), continue);
    if (!fs::Exists(abs_path_lib)) {
      LOG(WARNING) << absl::Substitute("Java agent lib path $0 not found.", lib);
      continue;
    }
    LOG(INFO) << absl::Substitute("JavaSymbolizer found agent lib $0.", abs_path_lib.string());
    abs_path_libs.push_back(abs_path_lib.string());
  }

  if (abs_path_libs.size() == 0) {
    LOG(WARNING) << "Java symbols are disabled; could not find any Java symbolization agent libs.";
    return native_symbolizer;
  }

  auto jsymbolizer =
      std::unique_ptr<JavaSymbolizer>(new JavaSymbolizer(absl::StrJoin(abs_path_libs, ",")));
  jsymbolizer->native_symbolizer_ = std::move(native_symbolizer);
  return std::unique_ptr<Symbolizer>(jsymbolizer.release());
}

void JavaSymbolizer::IterationPreTick() {
  native_symbolizer_->IterationPreTick();
  for (auto& [upid, ctx] : symbolization_contexts_) {
    ctx->set_requires_refresh();
  }
  num_attaches_remaining_this_iteration_ = FLAGS_number_attach_attempts_per_iteration;
  monitor_.ResetJavaProcessAttachTrackers();
}

void JavaSymbolizer::DeleteUPID(const struct upid_t& upid) {
  auto iter = symbolization_contexts_.find(upid);
  if (iter != symbolization_contexts_.end()) {
    // Remove the symbolization artifacts to clean up any persistent mounts that may
    // re-appear if the container for the target Java process is restarted.
    iter->second->RemoveArtifacts();

    // Release memory used for the Java symbol index.
    symbolization_contexts_.erase(iter);
  }

  symbolizer_functions_.erase(upid);
  native_symbolizer_->DeleteUPID(upid);
}

std::string_view JavaSymbolizer::Symbolize(JavaSymbolizationContext* ctx, const uintptr_t addr) {
  return ctx->Symbolize(addr);
}

Status JavaSymbolizer::CreateNewJavaSymbolizationContext(const struct upid_t& upid) {
  constexpr auto kIOFlags = std::ios::in | std::ios::binary;
  const std::filesystem::path symbol_file_path = java::StirlingSymbolFilePath(upid);
  auto symbol_file = std::make_unique<std::ifstream>(symbol_file_path, kIOFlags);

  if (symbol_file->fail()) {
    char const* const fmt = "Java attacher [pid=$0]: Could not open symbol file: $1.";
    return error::Internal(fmt, upid.pid, symbol_file_path.string());
  }

  DCHECK(symbolization_contexts_.find(upid) == symbolization_contexts_.end());

  const auto [iter, inserted] = symbolization_contexts_.try_emplace(upid, nullptr);
  DCHECK(inserted);
  if (inserted) {
    auto native_symbolizer_fn = native_symbolizer_->GetSymbolizerFn(upid);
    iter->second = std::make_unique<JavaSymbolizationContext>(upid, native_symbolizer_fn,
                                                              std::move(symbol_file));
  }
  auto& ctx = iter->second;

  auto fn = absl::bind_front(&JavaSymbolizer::Symbolize, this, ctx.get());
  symbolizer_functions_[upid] = fn;

  return Status::OK();
}

bool JavaSymbolizer::Uncacheable(const struct upid_t& upid) {
  if (symbolization_contexts_.find(upid) != symbolization_contexts_.end()) {
    // A Java symbolization context exists for this UPID.
    // Java symbols cannot be cached. Return true.
    return true;
  }

  if (active_attachers_.empty()) {
    return false;
  }

  const auto iter = active_attachers_.find(upid);
  if (iter == active_attachers_.end()) {
    return false;
  }

  auto& attacher = *iter->second;

  if (!attacher.Finished()) {
    constexpr auto kTimeOutForAttach = std::chrono::seconds{10};
    const auto now = std::chrono::steady_clock::now();
    const auto start_time = attacher.start_time();
    const auto elapsed_time = now - start_time;

    if (elapsed_time >= kTimeOutForAttach) {
      LOG(WARNING) << absl::Substitute("Java attacher [pid=$0]: Time-out.", upid.pid);
      active_attachers_.erase(upid);
    }
    return false;
  }

  if (!attacher.attached()) {
    // Fail, but still need to clean up the attacher.
    active_attachers_.erase(upid);
    LOG(WARNING) << absl::Substitute("Java attacher [pid=$0]: Attach failed.", upid.pid);
    return false;
  }

  // Successful attach; delete the attacher.
  active_attachers_.erase(upid);
  g_java_proc_attached_counter.Increment();

  // Attempt to open the symbol file and create a new Java symbolization context.
  const Status new_ctx_status = CreateNewJavaSymbolizationContext(upid);
  if (!new_ctx_status.ok()) {
    LOG(WARNING) << new_ctx_status.msg();
    return false;
  }

  // Successful open of the Java symbol file. A new symbolization context has been created,
  // and the symbol function has been updated. Return true!
  return true;
}

profiler::SymbolizerFn JavaSymbolizer::GetSymbolizerFn(const struct upid_t& upid) {
  auto fn_it = symbolizer_functions_.find(upid);
  if (fn_it != symbolizer_functions_.end()) {
    return fn_it->second;
  }

  // The underlying symbolization function is the fallback if we fail out at some point below,
  // and is also the fallback if eventually we do not find a Java symbol.
  auto native_symbolizer_fn = native_symbolizer_->GetSymbolizerFn(upid);

  using fs_path = std::filesystem::path;
  const system::ProcParser proc_parser;
  auto status_or_exe_path = proc_parser.GetExePath(upid.pid);

  if (!status_or_exe_path.ok()) {
    // Unable to get the read /prod/<pid> for target process.
    // Fall back to native symbolizer.
    symbolizer_functions_[upid] = native_symbolizer_fn;
    return native_symbolizer_fn;
  }
  const fs_path proc_exe = status_or_exe_path.ConsumeValueOrDie();

  if (DetectApplication(proc_exe) != Application::kJava) {
    // This process is not Java. Fall back to native symbolizer.
    symbolizer_functions_[upid] = native_symbolizer_fn;
    return native_symbolizer_fn;
  }

  if (!fs::Exists(FLAGS_stirling_profiler_px_jattach_path)) {
    char const* const msg = "Could not find binary px_jattach using path: $0.";
    LOG(WARNING) << absl::Substitute(msg, FLAGS_stirling_profiler_px_jattach_path);
    symbolizer_functions_[upid] = native_symbolizer_fn;
    return native_symbolizer_fn;
  }

  // Check java PreserveFramePointer flag in cmd for first time upids.
  if (symbolizer_functions_.find(upid) == symbolizer_functions_.end()) {
    const std::string cmdline = proc_parser.GetPIDCmdline(upid.pid);
    if (!cmdline.empty()) {
      const size_t pos = cmdline.find(symbolization::kJavaPreserveFramePointerOption);
      if (pos == std::string::npos) {
        monitor_.AppendSourceStatusRecord(
            "perf_profiler",
            error::Internal("Frame pointer not available in pid: $0, cmd: \"$1\". Preserve frame "
                            "pointers with the JDK option: $2.",
                            upid.pid, cmdline, symbolization::kJavaPreserveFramePointerOption),
            "Java Symbolization");
      }
    }
  }

  // This process is a java process, increment the counter.
  g_java_proc_counter.Increment();

  const std::filesystem::path symbol_file_path = java::StirlingSymbolFilePath(upid);

  if (fs::Exists(symbol_file_path)) {
    LOG(INFO) << absl::Substitute("Found a pre-existing symbol file for pid: $0", upid.pid);
    // Found a pre-existing symbol file. Attempt to use it.
    const Status new_ctx_status = CreateNewJavaSymbolizationContext(upid);
    if (!new_ctx_status.ok()) {
      // Something went wrong with the pre-existing symbol file. Fall back to native.
      // TODO(jps): should we delete the pre-existing file and attempt re-attach?
      LOG(WARNING) << new_ctx_status.msg();
      symbolizer_functions_[upid] = native_symbolizer_fn;
      return native_symbolizer_fn;
    }

    // We successfully opened a pre-existing Java symbol file.
    // Method CreateNewJavaSymbolizationContext() also updated the symbolizer functions map.
    // Our work here is done, return the Java symbolization function from the map.
    DCHECK(symbolizer_functions_.find(upid) != symbolizer_functions_.end());
    return symbolizer_functions_[upid];
  }

  // If a problem was detected at runtime, the Java symbolization feature flag can be set to false.
  if (!FLAGS_stirling_profiler_java_symbols) {
    // The perf profile source connector was instantiated with Java symbolization enabled,
    // but now it is disabled. We will not attempt to attach any more JVMTI agents.
    LOG_FIRST_N(INFO, 1) << "New Java process detected, but Java symbols disabled.";
    symbolizer_functions_[upid] = native_symbolizer_fn;
    return native_symbolizer_fn;
  }

  // Check if the /tmp mount in the target container has storage capacity
  // available for our symbolization libraries and symbol file.
  const auto capacity_status = SpaceAvailableForAgentLibsAndSymbolFile(upid);
  if (!capacity_status.ok()) {
    monitor_.AppendSourceStatusRecord("perf_profiler", capacity_status, "Java Symbolization");
    symbolizer_functions_[upid] = native_symbolizer_fn;
    return native_symbolizer_fn;
  }

  if (num_attaches_remaining_this_iteration_ == 0) {
    // We have reached our limit of JVMTI agent attaches for this iteration.
    // Early out w/ native symbolizer, but *do not* store this in the symbolizer functions
    // map (we want to try to inject an agent on the next iteration).
    return native_symbolizer_fn;
  }

  // Increment attempt counter, and notify the Stirling monitor, before trying to attach.
  g_java_proc_attach_attempted_counter.Increment();
  monitor_.NotifyJavaProcessAttach(upid);

  // Create an agent attacher and put it into the active attachers map.
  const auto [iter, inserted] = active_attachers_.try_emplace(upid, nullptr);
  DCHECK(inserted);
  if (inserted) {
    JavaProfilingProcTracker::GetSingleton()->Add(upid);

    // Creating an agent attacher will start a subprocess, px_jattach.
    // px_jattach is responsible for determining which JVMTI symbolization agent library to use
    // (libc or musl), and then invoking jattach to inject the symbolization agent to the
    // target Java process.
    iter->second = std::make_unique<java::AgentAttacher>(upid, agent_libs_);

    // Deduct one from our quota of attach attempts per iteration.
    --num_attaches_remaining_this_iteration_;
  }

  // We need this to be non-blocking; immediately return using the native symbolizer function.
  // The calling context can determine if the attacher is done using method SymbolsHaveChanged().
  symbolizer_functions_[upid] = native_symbolizer_fn;
  return native_symbolizer_fn;
}

}  // namespace stirling
}  // namespace px
