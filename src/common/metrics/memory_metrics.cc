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

#include <string>

#include <prometheus/gauge.h>
#include <prometheus/registry.h>

#include "src/common/metrics/memory_metrics.h"

#ifdef TCMALLOC
#include <gperftools/malloc_extension.h>
#endif

namespace px {
namespace metrics {
MemoryMetrics::MemoryMetrics(prometheus::Registry* registry, std::string tag_key,
                             std::string tag_value)
    : heap_size_bytes(prometheus::BuildGauge()
                          .Name("heap_size_bytes")
                          .Help("Size of the tcmalloc heap in bytes.")
                          .Register(*registry)
                          .Add({{tag_key, tag_value}})),
      heap_inuse_bytes(prometheus::BuildGauge()
                           .Name("heap_inuse_bytes")
                           .Help("Number of bytes of the heap that tcmalloc considered 'inuse' at "
                                 "the time of measurement.")
                           .Register(*registry)
                           .Add({{tag_key, tag_value}})),
      heap_free_bytes(prometheus::BuildGauge()
                          .Name("heap_free_bytes")
                          .Help("Number of bytes of the heap that have been freed to tcmalloc but "
                                "not released to the OS.")
                          .Register(*registry)
                          .Add({{tag_key, tag_value}})) {}

void MemoryMetrics::MeasureMemory() {
#ifdef TCMALLOC
  auto instance = MallocExtension::instance();
  size_t inuse_bytes, heap_size, pageheap_free_bytes, central_cache_free_bytes,
      transfer_cache_free_bytes, thread_cache_free_bytes;
  instance->GetNumericProperty("generic.current_allocated_bytes", &inuse_bytes);
  instance->GetNumericProperty("generic.heap_size", &heap_size);
  instance->GetNumericProperty("tcmalloc.pageheap_free_bytes", &pageheap_free_bytes);
  instance->GetNumericProperty("tcmalloc.central_cache_free_bytes", &central_cache_free_bytes);
  instance->GetNumericProperty("tcmalloc.transfer_cache_free_bytes", &transfer_cache_free_bytes);
  instance->GetNumericProperty("tcmalloc.thread_cache_free_bytes", &thread_cache_free_bytes);

  heap_size_bytes.Set(static_cast<double>(heap_size));
  heap_inuse_bytes.Set(static_cast<double>(inuse_bytes));
  heap_free_bytes.Set(static_cast<double>(pageheap_free_bytes + central_cache_free_bytes +
                                          transfer_cache_free_bytes + thread_cache_free_bytes));
#endif
}

}  // namespace metrics
}  // namespace px
