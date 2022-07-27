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

#include <memory>
#include <string>

#include <absl/functional/bind_front.h>

#include "src/stirling/bpf_tools/bcc_symbolizer.h"
#include "src/stirling/source_connectors/perf_profiler/symbolizers/bcc_symbolizer.h"

namespace px {
namespace stirling {

StatusOr<std::unique_ptr<Symbolizer>> BCCSymbolizer::Create() {
  return std::unique_ptr<Symbolizer>(new BCCSymbolizer());
}

void BCCSymbolizer::DeleteUPID(const struct upid_t& upid) {
  // Free up the symbol cache on the BCC side.
  // If the BCC side symbol cache has already been freed, this does nothing.
  // If later the pid is reused, then BCC will re-allocate the pid's symbol
  // symbol cache (when get_addr_symbol() is called).
  bcc_symbolizer_.ReleasePIDSymCache(upid.pid);
}

profiler::SymbolizerFn BCCSymbolizer::GetSymbolizerFn(const struct upid_t& upid) {
  auto fn = absl::bind_front(&bpf_tools::BCCSymbolizer::SymbolOrAddrIfUnknown, &bcc_symbolizer_,
                             static_cast<int32_t>(upid.pid));
  return fn;
}

}  // namespace stirling
}  // namespace px
