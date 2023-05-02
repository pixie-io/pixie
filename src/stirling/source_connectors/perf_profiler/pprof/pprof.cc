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

#include "src/stirling/source_connectors/perf_profiler/pprof/pprof.h"

#include <gflags/gflags.h>

#include <absl/strings/str_split.h>
#include <vector>

DECLARE_uint32(stirling_profiler_stack_trace_sample_period_ms);

namespace px {
namespace stirling {

PProfProfile CreatePProfProfile(const uint32_t num_cpus, const histo_t& histo) {
  // Info on the pprof proto format:
  // https://github.com/google/pprof/blob/main/proto/profile.proto

  // period_ns will be used when populating the nanos count.
  const uint64_t period_ms = FLAGS_stirling_profiler_stack_trace_sample_period_ms;
  const uint64_t period_ns = period_ms * 1000 * 1000;

  // Tracks which strings have been inserted into the profile.
  absl::flat_hash_map<std::string, uint64_t> strings;

  // This is the pprof profile.
  ::perftools::profiles::Profile profile;

  // The pprof profiles start by describing their sample types. For CPU profiling,
  // the convention is to describe two different metrics:
  // "samples" with units of "count", and
  // "cpu" with units of "nanoseconds".
  // Note, the first entry in the strings table is required to be an empty string.
  auto sample_type = profile.add_sample_type();
  sample_type->set_type(1);
  sample_type->set_unit(2);
  profile.add_string_table("");
  profile.add_string_table("samples");
  profile.add_string_table("count");
  sample_type = profile.add_sample_type();
  sample_type->set_type(3);
  sample_type->set_unit(4);
  profile.add_string_table("cpu");
  profile.add_string_table("nanoseconds");

  // State variables useful as we build the pprof profile message.
  // No locations messages exist (yet); next_location_id starts at 1.
  // We already added some strings; next_string_id starts at 5.
  uint64_t next_location_id = 1;
  uint64_t next_string_id = 5;

  // To build the profile, we iterate over the stack traces  histogram.
  for (const auto& [stack_trace_str, count] : histo) {
    // Each histo entry will be recorded as a new sample.
    auto sample = profile.add_sample();

    // That sample will record its count and time in nanos.
    const uint64_t nanos = count * period_ns / num_cpus;
    sample->add_value(count);
    sample->add_value(nanos);

    // Our stack traces are symbolized like so: main;foo;bar
    // thus, we split on ';' to iterate over the individual symbols.
    const std::vector<std::string_view> symbols = absl::StrSplit(stack_trace_str, ";");

    // Iterate over the symbols in this stack trace.
    // Each symbol will be added to the sample as a "location" message, which in turn refers to a
    // string in the strings table (tracked in map "strings") (more details below).
    // Note, because of how we built the stack trace string and how the pprof profile is
    // organized, we iterate in reverse order.
    for (auto symbols_iter = symbols.rbegin(); symbols_iter != symbols.rend(); ++symbols_iter) {
      // For convenience.
      const auto& symbol = *symbols_iter;

      // try_emplace() looks up an existing entry in the strings table, or creates a new entry
      // with the key provided. Here a new entry maps from symbol to next_location_id.
      const auto [strings_iter, inserted] = strings.try_emplace(symbol, next_location_id);

      if (inserted) {
        // New symbol: create a new location, add it to the sample, and create a new symbol.

        // For convenience & clarity, store the ids in locals, then compute their next values.
        const uint64_t location_id = next_location_id;
        const uint64_t string_id = next_string_id;
        ++next_location_id;
        ++next_string_id;

        // Add a new "location" to the profile.
        // Each sample is a sequence of locations. The locations, in their sequence, represent a
        // stack trace. Each location may include an address and a reference to a mapping (useful
        // if symbols are not included in the profile, enables post-hoc symbolization).
        // Lacking both address and mapping here, we skip those.
        auto location = profile.add_location();
        location->set_id(location_id);

        // To connect a location to a symbol, we need to go through a "line" and a "function".

        // Add a "line" message to the location message.
        // In our usage, this is essentially a pointer to a function message that points to a
        // symbol. The function message may contain more information (e.g. starting line number).
        auto line = location->add_line();
        line->set_function_id(location_id);

        // Add a "function" to the profile (to point to the string table).
        auto function = profile.add_function();
        function->set_id(location_id);

        // Add a reference to the string table entry in the function message.
        function->set_name(string_id);

        // Add the string to the string table in the profile.
        profile.add_string_table(std::string(symbol));

        // Add the new location-id into the sample in the profile.
        sample->add_location_id(location_id);

        // Record the fact that we have written this string into the profile. Save the
        // location-id because it is used if we find this symbol again.
        strings[symbol] = location_id;
      } else {
        // Existing symbol.
        // Just place the pre-existing location id into the sample.
        const uint64_t location_id = strings_iter->second;
        sample->add_location_id(location_id);
      }
    }
  }
  return profile;
}

}  // namespace stirling
}  // namespace px
