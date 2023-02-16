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

#include "src/carnot/exec/exec_metrics.h"
#include <prometheus/counter.h>
#include <string>

ExecMetrics::ExecMetrics(prometheus::Registry* registry)
    : otlp_metrics_timeout_counter(
          prometheus::BuildCounter()
              .Name("otlp_timeouts")
              .Help("Total number of timeouts which occurred when exporting data to an OTLP client")
              .Register(*registry)
              .Add({{"name", "metrics"}})),
      otlp_spans_timeout_counter(
          prometheus::BuildCounter()
              .Name("otlp_timeouts")
              .Help("Total number of timeouts which occurred when exporting data to an OTLP client")
              .Register(*registry)
              .Add({{"name", "spans"}})) {}
