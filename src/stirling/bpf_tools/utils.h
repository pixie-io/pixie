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

#include <vector>

#include "src/common/base/statusor.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"
#include "src/stirling/obj_tools/elf_reader.h"

namespace px {
namespace stirling {
namespace bpf_tools {

/**
 * Returns a list of UProbeSpec duplicated from the input UProbeSpec, whose addresses are the return
 * addresses of the function specified by the target. The input elf_reader is used to parse the
 * binary.
 */
StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(
    const UProbeSpec& spec, const obj_tools::ElfReader::SymbolInfo& target,
    obj_tools::ElfReader* elf_reader);

/**
 * Wraps the above function. The additional functionality is to obtain SymbolInfo from the symbol
 * address of the input spec.
 */
StatusOr<std::vector<UProbeSpec>> TransformGolangReturnProbe(const UProbeSpec& spec,
                                                             obj_tools::ElfReader* elf_reader);

/**
 * Update BPF per-cpu array value at the specified index.
 * Note that the value is actually written onto all CPUs by BCC.
 */
template <typename TValueType>
Status UpdatePerCPUArrayValue(int idx, TValueType val, ebpf::BPFPercpuArrayTable<TValueType>* arr) {
  std::vector<TValueType> values(bpf_tools::BCCWrapper::kCPUCount, val);
  auto update_res = arr->update_value(idx, values);
  if (!update_res.ok()) {
    return error::Internal(absl::Substitute("Failed to set value on index: $0, error message: $1",
                                            idx, update_res.msg()));
  }
  return Status::OK();
}

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
