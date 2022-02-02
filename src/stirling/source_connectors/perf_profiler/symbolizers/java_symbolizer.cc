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
#include <utility>
#include <vector>

#include "src/common/system/proc_parser.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/source_connectors/perf_profiler/java/demangle.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/java_symbolizer.h"
#include "src/stirling/utils/detect_application.h"

namespace px {
namespace stirling {

StatusOr<std::unique_ptr<Symbolizer>> JavaSymbolizer::Create(
    std::unique_ptr<Symbolizer> native_symbolizer) {
  auto java_symbolizer = std::unique_ptr<JavaSymbolizer>(new JavaSymbolizer);
  java_symbolizer->native_symbolizer_ = std::move(native_symbolizer);
  return std::unique_ptr<Symbolizer>(java_symbolizer.release());
}

void JavaSymbolizer::DeleteUPID(const struct upid_t& upid) {
  // The inner map is owned by a unique_ptr; this will free the memory.
  symbolizer_functions_.erase(upid);
  native_symbolizer_->DeleteUPID(upid);
}

std::string_view JavaSymbolizer::Symbolize(const uintptr_t /*addr*/) {
  // TODO(jps): This is currently a placeholder. A future diff will
  // fill in the actual symbolization logic.
  constexpr std::string_view placeholder = "[j] <symbol>";
  return placeholder;
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
  const auto& proc_parser = system::ProcParser(system::Config::GetInstance());
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

  const std::vector<std::string> libs = {
      "/pl/lib-px-java-agent-musl.so",
      "/pl/lib-px-java-agent-glibc.so",
  };
  const std::string kSymFilePathPfx = "/tmp/px-java-symbolization-agent";

  auto attacher = java::AgentAttacher(upid.pid, kSymFilePathPfx, libs);

  constexpr auto kTimeOutForAttach = std::chrono::milliseconds{250};
  constexpr auto kAttachRecheckPeriod = std::chrono::milliseconds{10};
  auto time_elapsed = std::chrono::milliseconds{0};

  while (!attacher.Finished()) {
    if (time_elapsed >= kTimeOutForAttach) {
      // Attacher did not complete. Fall back to native symbolizer.
      symbolizer_functions_[upid] = native_symbolizer_fn;
      return native_symbolizer_fn;
    }
    // Still waiting to finish the attach process.
    // TODO(jps): Create a temporary symbolization function,
    // and return that here to unblock Stirling.
    std::this_thread::sleep_for(kAttachRecheckPeriod);
    time_elapsed += kAttachRecheckPeriod;
  }

  if (!attacher.attached()) {
    // This process *is* Java, but we failed to attach the symbolization agent. Fall back to
    // symbolizer function from the underlying native symbolizer.
    // To prevent this from happening again, store that in the map.
    symbolizer_functions_[upid] = native_symbolizer_fn;
    return native_symbolizer_fn;
  }

  using std::placeholders::_1;
  auto fn = std::bind(&JavaSymbolizer::Symbolize, this, _1);

  symbolizer_functions_[upid] = fn;
  return fn;
}

}  // namespace stirling
}  // namespace px
