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

#pragma once

#include <string>
#include <vector>

// This depends on LLVM, which has conflicting symbols with ElfReader.
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/physicalpb/physical.pb.h"

namespace px {
namespace stirling {
namespace dynamic_tracing {

// For non-base types, like strings and arrays, we currently used fixed size objects
// to transfer the data. Anything larger will be truncated.
// These constants define the "container" size for these non-base types.

// NOTE: There are three separate sizes for strings, byte arrays and blobs.
//       This is currently to accommodate two limitations:
//         1) These structures are on the stack, and we want to tune for BPF stack usage.
//         2) Our perf_submit is of a fixed sized equal to the size of the struct (instead of
//            actual usage).
//
// When both these limitations are fixed, these sizes can be converged into a single size.

// Fixed size for tracing strings.
constexpr size_t kStructStringSize = 32;

// Fixed size for tracing byte arrays.
constexpr size_t kStructByteArraySize = 64;

// Fixed size for tracing structs.
// TODO(yzhao): This size must match that of the dynamic generated struct during compilation.
// That has to be managed manually in the code. We should think about deriving size from the
// generated types.
constexpr size_t kStructBlobSize = 64;

struct BCCProgram {
  struct PerfBufferSpec {
    std::string name;
    ir::physical::Struct output;

    std::string ToString() const {
      return absl::Substitute("[name=$0 Output struct=$1]", name, output.DebugString());
    }
  };

  std::vector<bpf_tools::UProbeSpec> uprobe_specs;
  std::vector<PerfBufferSpec> perf_buffer_specs;
  std::string code;

  std::string ToString() const {
    std::string txt;

    for (const auto& spec : uprobe_specs) {
      absl::StrAppend(&txt, spec.ToString(), "\n");
    }
    for (const auto& spec : perf_buffer_specs) {
      absl::StrAppend(&txt, spec.ToString(), "\n");
    }
    absl::StrAppend(&txt, "[BCC BEGIN]\n", code, "\n[BCC END]");

    return txt;
  }
};

}  // namespace dynamic_tracing
}  // namespace stirling
}  // namespace px
