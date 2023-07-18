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

#include <bcc/BPF.h>
// Including bcc/BPF.h creates some conflicts with llvm.
// So must remove this stray define for things to work.
#ifdef STT_GNU_IFUNC
#undef STT_GNU_IFUNC
#endif

// Including bcc/BPF.h creates some conflicts with our own code.
#ifdef DECLARE_ERROR
#undef DECLARE_ERROR
#endif

#include <linux/perf_event.h>

#include <filesystem>
#include <string>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/common/json/json.h"

namespace px {
namespace stirling {
namespace bpf_tools {

enum class BPFProbeAttachType {
  // Attach to function entry.
  kEntry = BPF_PROBE_ENTRY,
  // Attach to function return (BCC native way, using stack).
  kReturn = BPF_PROBE_RETURN,
  // Attach to all function return instructions (required for golang).
  kReturnInsts,
};

/**
 * Describes a kernel probe (kprobe).
 */
struct KProbeSpec {
  // Name of kernel function to probe (currently must be syscall).
  std::string_view kernel_fn;

  // Whether this is an ENTRY or RETURN probe.
  BPFProbeAttachType attach_type = BPFProbeAttachType::kEntry;

  // Name of user-provided function to run when event is triggered.
  std::string_view probe_fn;

  // If true the kernel_fn is the short name of a syscall.
  bool is_syscall = true;

  // Whether to fail if the kprobe doesn't deploy. Useful in case the symbol may not exist in some
  // kernels.
  bool is_optional = false;

  std::string ToString() const {
    return absl::Substitute("[kernel_function=$0 type=$1 probe=$2]", kernel_fn,
                            magic_enum::enum_name(attach_type), probe_fn);
  }
};

/**
 * Describes a userspace probe (uprobe).
 */
struct UProbeSpec {
  // The canonical path to the binary to which this uprobe is attached.
  std::filesystem::path binary_path;

  // Exactly one of symbol and address must be specified.
  std::string symbol;
  uint64_t address = 0;

  // Must be identical to the default value of `pid` argument of BPF::{attach,detach}_uprobe().
  static constexpr pid_t kDefaultPID = -1;

  // Specifies the target process to attach. This still requires setting binary_path, symbol or
  // address.
  pid_t pid = kDefaultPID;

  BPFProbeAttachType attach_type = BPFProbeAttachType::kEntry;
  std::string probe_fn;
  bool is_optional = false;

  std::string ToString() const {
    return absl::Substitute(
        "[binary=$0 symbol=$1 address=$2 pid=$3 type=$4 probe_fn=$5 optional=$6]",
        binary_path.string(), symbol, address, pid, magic_enum::enum_name(attach_type), probe_fn,
        is_optional);
  }

  std::string ToJSON() const {
    ::px::utils::JSONObjectBuilder builder;
    builder.WriteKV("binary", binary_path.string());
    builder.WriteKV("symbol", symbol);
    builder.WriteKV("address", static_cast<int64_t>(address));
    builder.WriteKV("pid", pid);
    builder.WriteKV("type", magic_enum::enum_name(attach_type));
    builder.WriteKV("probe_fn", probe_fn);
    return builder.GetString();
  }
};

/**
 * Describes a probe on a pre-defined kernel tracepoint.
 */
struct TracepointSpec {
  std::string tracepoint;
  std::string probe_fn;

  std::string ToString() const {
    return absl::Substitute("[tracepoint=$0 probe=$1]", tracepoint, probe_fn);
  }
};

/**
 * Describes a sampling probe that triggers according to a time period.
 * This is in contrast to KProbes and UProbes, which trigger based on
 * a code event.
 */
struct SamplingProbeSpec {
  // Name of user-provided BPF function to run when probe is triggered.
  std::string_view probe_fn;

  // Sampling period in milliseconds to trigger the probe.
  uint64_t period_millis;
};

/**
 * PerfBufferSizeCategory specifies which category (currently Data or Control) a perf buffer belongs
 * to. This is used for accounting purposes, so that a maximum total size can be set per category.
 */
enum class PerfBufferSizeCategory {
  kUncategorized,
  kData,
  kControl,
};

/**
 * Describes a BPF perf buffer, through which data is returned to user-space.
 */
struct PerfBufferSpec {
  // Name of the perf buffer.
  // Must be the same as the perf buffer name declared in the probe code with BPF_PERF_OUTPUT.
  std::string name;

  // Function that will be called for every event in the perf buffer,
  // when perf buffer read is triggered.
  perf_reader_raw_cb probe_output_fn;

  // Function that will be called if there are lost/clobbered perf events.
  perf_reader_lost_cb probe_loss_fn;

  // Size of perf buffer. Will be rounded up to and allocated in a power of 2 number of pages.
  int size_bytes = 1024 * 1024;

  // We specify a maximum total size per PerfBufferSizeCategory, this specifies which size category
  // to count this buffer's size against.
  PerfBufferSizeCategory size_category = PerfBufferSizeCategory::kUncategorized;

  std::string ToString() const {
    return absl::Substitute("name=$0 size_bytes=$1 size_category=$2", name, size_bytes,
                            magic_enum::enum_name(size_category));
  }
};

/**
 * Describes a perf event to attach.
 * This can be run stand-alone and is not dependent on kProbes.
 */
struct PerfEventSpec {
  // The type of perf event (e.g. PERF_TYPE_HARDWARE, PERF_TYPE_SOFTWARE, etc.)
  perf_type_id type;

  // The actual event to be counted (e.g. PERF_COUNT_HW_CPU_CYCLES).
  uint32_t config;

  // Name of user-provided function to run when event is triggered.
  std::string_view probe_fn;

  // Sampling period in number of events.
  // Mutually exclusive with sample_freq.
  uint64_t sample_period;
};

}  // namespace bpf_tools
}  // namespace stirling
}  // namespace px
