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

#include <prometheus/registry.h>

#include "src/common/metrics/metrics.h"

namespace {
std::unique_ptr<prometheus::Registry> g_registry_instance;

void ResetMetricsRegistry() { g_registry_instance = std::make_unique<prometheus::Registry>(); }
}  // namespace

prometheus::Registry& GetMetricsRegistry() {
  if (g_registry_instance == nullptr) {
    ResetMetricsRegistry();
  }
  return *g_registry_instance;
}

void TestOnlyResetMetricsRegistry() { ResetMetricsRegistry(); }
