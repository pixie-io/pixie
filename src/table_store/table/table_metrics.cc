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

#include "src/table_store/table/table_metrics.h"
#include <prometheus/counter.h>
#include <string>

TableMetrics::TableMetrics(prometheus::Registry* registry, std::string table_name)
    : bytes_added_counter(prometheus::BuildCounter()
                              .Name("table_bytes_added")
                              .Help("Total bytes written to the table in the table's lifetime")
                              .Register(*registry)
                              .Add({{"name", table_name}})),
      cold_bytes_gauge(prometheus::BuildGauge()
                           .Name("table_cold_bytes")
                           .Help("Current cold data bytes in the table")
                           .Register(*registry)
                           .Add({{"name", table_name}})),
      hot_bytes_gauge(prometheus::BuildGauge()
                          .Name("table_hot_bytes")
                          .Help("Current hot data bytes in the table")
                          .Register(*registry)
                          .Add({{"name", table_name}})),
      num_batches_gauge(prometheus::BuildGauge()
                            .Name("table_num_batches")
                            .Help("Current number of row batches in the table")
                            .Register(*registry)
                            .Add({{"name", table_name}})),
      batches_added_counter(prometheus::BuildCounter()
                                .Name("table_batches_added")
                                .Help("Total batches added to the table in the table's lifetime")
                                .Register(*registry)
                                .Add({{"name", table_name}})),
      batches_expired_counter(
          prometheus::BuildCounter()
              .Name("table_batches_expired")
              .Help("Total batches expired from the table in the table's lifetime")
              .Register(*registry)
              .Add({{"name", table_name}})),
      compacted_batches_counter(
          prometheus::BuildCounter()
              .Name("table_compacted_batches")
              .Help("Total batches compacted in the table in the table's lifetime")
              .Register(*registry)
              .Add({{"name", table_name}})),
      max_table_size_gauge(prometheus::BuildGauge()
                               .Name("table_max_table_size")
                               .Help("The cap on the table size")
                               .Register(*registry)
                               .Add({{"name", table_name}})),
      retention_ns_gauge(prometheus::BuildGauge()
                             .Name("min_time")
                             .Help("The current retention window for data in this table")
                             .Register(*registry)
                             .Add({{"name", table_name}})) {}
