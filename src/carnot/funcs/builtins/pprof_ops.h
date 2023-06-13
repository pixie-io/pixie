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
            R"doc(
        | # Get the stack traces, the underlying data we want; populate an ASID column
        | # to join with profiler sampling period (see next).
        | stack_traces = px.DataFrame(table='stack_traces.beta', start_time='-1m')
        | stack_traces.asid = px.asid()
        |
        | # Get the profiler sampling period for all deployed PEMs, then merge to stack traces on ASID.
        | sample_period = px.GetProfilerSamplingPeriodMS()
        | df = stack_traces.merge(sample_period, how='inner', left_on=['asid'], right_on=['asid'])
        |
        | # The pprof UDA requires that each underlying dataset have the same sampling period.
        | # Thus, group by sampling period (normally this results in just one group).
        | df = df.groupby(['profiler_sampling_period_ms']).agg(pprof=('stack_trace', 'count', 'profiler_sampling_period_ms', px.pprof))
        )doc")
        .Arg("stack_trace", "Stack trace string.")
        .Arg("count", "Count of the stack trace string.")
        .Arg("profiler_period_ms", "Profiler stack trace sampling period in ms.")
        .Returns("A single row that aggregates all the stack traces and counts into pprof format.");
  }

  void Update(FunctionContext*, const StringValue stack_trace, const Int64Value count,
              const Int64Value profiler_period_ms) {
    UpdateOrCheckSamplingPeriod(profiler_period_ms.val);

    histo_[stack_trace] += count.val;
  }

  void Merge(FunctionContext*, const CreatePProfRowAggregate& other) {
    UpdateOrCheckSamplingPeriod(other.profiler_period_ms_);

    for (const auto& [stack_trace, count] : other.histo_) {
      histo_[stack_trace] += count;
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

  Status Deserialize(FunctionContext*, const StringValue& pprof_str) {
    // Parse serialized input a pprof proto object.
    PProfProfile pprof;
    if (!pprof.ParseFromString(pprof_str)) {
      return error::Internal("Could not parse input string into a pprof proto.");
    }

    UpdateOrCheckSamplingPeriod(pprof.period() / 1000 / 1000);

    // Deserialize into a map from stack_trace string to count.
    const auto merge_histo = ::px::shared::DeserializePProfProfile(pprof);

    // Incorporate the deserialized result into our histo_.
    for (const auto& [stack_trace, count] : merge_histo) {
      histo_[stack_trace] += count;
    }
    return Status::OK();
  }

  StringValue Finalize(FunctionContext* ctx) { return Serialize(ctx); }

 protected:
  void UpdateOrCheckSamplingPeriod(const int32_t profiler_period_ms) {
    // Initialize profiler_period_ms_ if needed.
    if (profiler_period_ms_ == -1) {
      profiler_period_ms_ = profiler_period_ms;
    }

    // If any inconsistent profiler period is observed, set the error flag.
    if (profiler_period_ms_ != profiler_period_ms) {
      multiple_profiler_periods_found_ = true;
    }
  }

  absl::flat_hash_map<std::string, uint64_t> histo_;
  int32_t profiler_period_ms_ = -1;
  bool multiple_profiler_periods_found_ = false;
};

void RegisterPProfOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
