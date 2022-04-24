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
#include <prometheus/registry.h>
#include <string>

#include "src/common/metrics/metrics.h"

struct TableMetrics {
  TableMetrics(prometheus::Registry* registry, std::string table_name);

  prometheus::Counter& bytes_added_counter;
  prometheus::Gauge& cold_bytes_gauge;
  prometheus::Gauge& hot_bytes_gauge;
  prometheus::Gauge& num_batches_gauge;
  prometheus::Counter& batches_added_counter;
  prometheus::Counter& batches_expired_counter;
  prometheus::Counter& compacted_batches_counter;
  prometheus::Gauge& max_table_size_gauge;
  prometheus::Gauge& retention_ns_gauge;
};
