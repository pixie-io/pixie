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

#include "src/shared/pprof/pprof.h"

#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace shared {

PProfProfile CreatePProfProfile(const uint32_t period_ms, const PProfHisto& histo) {
  // Info on the pprof proto format:
  // https://github.com/google/pprof/blob/main/proto/profile.proto

  // period_ms is the stack trace sampling period used by the eBPF stack trace sampling probe.
  // period_ns will be used when populating the nanos count.
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

  // Store the underlying stack trace sampling period.
  auto period_type = profile.mutable_period_type();
  period_type->set_type(3);
  period_type->set_unit(4);
  profile.set_period(period_ns);

  // State variables useful as we build the pprof profile message.
  // No locations messages exist (yet); next_location_id starts at 1.
  // We already added some strings; next_string_id starts at 5.
  uint64_t next_location_id = 1;
  uint64_t next_string_id = 5;

  // To build the profile, we iterate over the stack traces histogram.
  for (const auto& [stack_trace_str, count] : histo) {
    // Each histo entry will be recorded as a new sample.
    auto sample = profile.add_sample();

    // That sample will record its count and time in nanos.
    const uint64_t nanos = count * period_ns;
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
      const auto& symbol = *symbols_iter;

      // try_emplace() looks up an existing entry in the strings table, or creates a new entry
      // with the key provided. Here a new entry maps from symbol to next_location_id.
      const auto [strings_iter, inserted] = strings.try_emplace(symbol, next_location_id);

      if (inserted) {
        // New symbol: create a new location, add it to the sample, and create a new symbol.

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

absl::flat_hash_map<std::string, uint64_t> DeserializePProfProfile(const PProfProfile& pprof) {
  // This function reads from the protobuf pprof to populate and return this stack trace histogram.
  absl::flat_hash_map<std::string, uint64_t> histo;

  // Iterate over each sample to find the underlying stack trace string and its count.
  for (const auto& sample : pprof.sample()) {
    // Collect symbols into this vector.
    std::vector<std::string> symbols;

    // Iterate through the symbols, i.e. the stack trace locations. Two notes...
    // 1. Our stack trace strings (e.g. "main;compute;leaf") are in reversed in order vs. pprof.
    // 2. PProf proto locations are 1 indexed but C++ is 0 indexed, hence the "-1" offset applied to
    // the indices below. With 0 indexing, `go tool pprof` refused to render the pprof with
    // the following message: malformed profile: found function with reserved ID=0.
    auto& location_ids = sample.location_id();
    for (auto id_iter = location_ids.rbegin(); id_iter != location_ids.rend(); id_iter++) {
      const auto& location = pprof.location(*id_iter - 1);

      // Each location points to a line, which points to a function, which points to a symbol.
      DCHECK_EQ(location.line_size(), 1);
      const auto& line = location.line(0);
      const auto& function = pprof.function(line.function_id() - 1);
      const auto& symbol = pprof.string_table(function.name());

      // Found a symbol; store it.
      symbols.push_back(symbol);
    }

    // There are two values for each sample: count & nanos. Here, we ignore the nanos value because
    // it cannot be stored in the histogram that this function produces.
    DCHECK_EQ(sample.value_size(), 2);
    const uint64_t count = sample.value(0);
    const std::string stack_trace = absl::StrJoin(symbols, ";");
    histo[stack_trace] = count;
  }
  return histo;
}

}  // namespace shared
}  // namespace px
