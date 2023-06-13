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

#include <absl/container/flat_hash_map.h>

#include <string>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/shared/pprof/pprof.h"

namespace px {
namespace carnot {
namespace builtins {

using px::shared::PProfProfile;

class CreatePProfRowAggregate : public udf::UDA {
 public:
  static udf::UDADocBuilder Doc() {
    return udf::UDADocBuilder("Convert perf profiling data to pprof format.")
        .Details("Converts perf profiling stack traces into pprof format.")
        .Example(
            R"example(df = px.DataFrame(table='stack_traces.beta', start_time='-1m')
df = df.agg(pprof=('stack_trace', 'count', px.pprof)))example")
        .Arg("stack_trace", "Stack trace string.")
        .Arg("count", "Count of the stack trace string.")
        .Arg("profiler_period_ms", "Profiler stack trace sampling period in ms.")
        .Returns("A single row that aggregates all the stack traces and counts into pprof format.");
  }

  void Update(FunctionContext*, const StringValue stack_trace, const Int64Value count,
              const Int64Value profiler_period_ms) {
    histo_[stack_trace] += count.val;

    // Initialize profiler_period_ms_.
    if (profiler_period_ms_ == -1) {
      profiler_period_ms_ = profiler_period_ms.val;
    }

    // If any inconsistent profiler period is observed, set the error flag.
    if (profiler_period_ms_ != profiler_period_ms.val) {
      multiple_profiler_periods_found_ = true;
    }
  }

  void Merge(FunctionContext*, const CreatePProfRowAggregate& other) {
    for (const auto& [stack_trace, count] : other.histo_) {
      histo_[stack_trace] += count;
    }
    if (profiler_period_ms_ != other.profiler_period_ms_) {
      multiple_profiler_periods_found_ = true;
    }
  }

  StringValue Serialize(FunctionContext*) {
    if (multiple_profiler_periods_found_) {
      return "Protobuf `SerializeToString` failed, multiple profiling periods found.";
    }

    const auto pprof = px::shared::CreatePProfProfile(profiler_period_ms_, histo_);
    std::string output;
    const bool ok = pprof.SerializeToString(&output);
    if (!ok) {
      return "Protobuf `SerializeToString` failed.";
    }
    return output;
  }

  Status Deserialize(FunctionContext*, const StringValue& upstream_pprof_str) {
    // Parse upstream data into a pprof proto object.
    PProfProfile pprof;
    if (!pprof.ParseFromString(upstream_pprof_str)) {
      return error::Internal("Could not parse input string into a pprof proto.");
    }

    if (profiler_period_ms_ == -1) {
      // Initialize profiler_period_ms_ from the upstream pprof.
      profiler_period_ms_ = pprof.period() / 1000 / 1000;
    } else {
      // Verify consistency of profiler_period_ms_ with the upstream pprof.
      const int64_t profiler_period_ns = profiler_period_ms_ * 1000 * 1000;
      if (profiler_period_ns != pprof.period()) {
        multiple_profiler_periods_found_ = true;
      }
    }

    // Deserialize into a map from stack_trace string to count.
    const auto upstream_histo = ::px::shared::DeserializePProfProfile(pprof);

    // Incorporate the deserialized result into our histo_.
    for (const auto& [stack_trace, count] : upstream_histo) {
      histo_[stack_trace] += count;
    }
    return Status::OK();
  }

  StringValue Finalize(FunctionContext* ctx) { return Serialize(ctx); }

 protected:
  absl::flat_hash_map<std::string, uint64_t> histo_;
  int32_t profiler_period_ms_ = -1;
  bool multiple_profiler_periods_found_ = false;
};

void RegisterPProfOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
