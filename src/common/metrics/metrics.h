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

#include <prometheus/counter.h>
#include <prometheus/registry.h>

// Returns the global metrics registry;
prometheus::Registry& GetMetricsRegistry();

// Resets the Metrics registry, removing all of its contained metrics.
// This function should only be called by testing code.
void TestOnlyResetMetricsRegistry();

// A convenience wrapper to return a counter with the specified name and help message when
// dimensions aren't known at compile time. This should only be used when dimensional data is
// very low cardinality.
inline auto& BuildCounterFamily(const std::string& name, const std::string& help_message) {
  return prometheus::BuildCounter().Name(name).Help(help_message).Register(GetMetricsRegistry());
}

// A convenience wrapper to return a counter with the specified name and help message.
inline auto& BuildCounter(const std::string& name, const std::string& help_message) {
  return prometheus::BuildCounter()
      .Name(name)
      .Help(help_message)
      .Register(GetMetricsRegistry())
      .Add({{"name", name}});
}

// A convenience wrapper to return a gauge with the specified name and help message.
inline auto& BuildGauge(const std::string& name, const std::string& help_message) {
  return prometheus::BuildGauge()
      .Name(name)
      .Help(help_message)
      .Register(GetMetricsRegistry())
      .Add({{"name", name}});
}
