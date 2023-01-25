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

#include <benchmark/benchmark.h>

#include "experimental/stirling/proto_data_model/http_record.h"
#include "experimental/stirling/proto_data_model/proto/http_record.pb.h"
#include "src/shared/types/column_wrapper.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/types.h"

namespace px {

const size_t kRangeMultiplier = 10;
const size_t kRangeBegin = 100;
const size_t kRangeEnd = 10'000'000;

using ::experimental::HTTPRecord;
using ::px::Status;
using ::px::stirling::kHTTPElements;
using ::px::types::ColumnWrapperRecordBatch;

// Initializes record_batch so that it has data fields that matches data_elements' spec.
// Template to cover both ConstVectorView<DataElement> and std::vector<InfoClassElement>.
// TODO(oazizi): No point in returning a Status for this function.
template <class T>
Status InitRecordBatch(const T& data_elements, uint32_t target_capacity,
                       ColumnWrapperRecordBatch* record_batch) {
  for (const auto& element : data_elements) {
    px::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(target_capacity);                  \
  record_batch->push_back(col);
    PX_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
  return Status::OK();
}

// NOLINTNEXTLINE : runtime/references.
static void BM_record_batch_pb(benchmark::State& state) {
  HTTPRecord record;
  ColumnWrapperRecordBatch record_batch;
  PX_CHECK_OK(InitRecordBatch(kHTTPElements, state.range(0), &record_batch));
  for (auto _ : state) {
    for (auto& element : record_batch) {
      element->Clear();
    }
    for (size_t i = 0; i < static_cast<size_t>(state.range(0)); i++) {
      ConsumeHTTPRecord(record, &record_batch);
    }
    PX_UNUSED(_);
  }
  state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(HTTPRecord));
}

// NOLINTNEXTLINE : runtime/references.
static void BM_record_batch_pb_refl(benchmark::State& state) {
  HTTPRecord record;
  ColumnWrapperRecordBatch record_batch;
  PX_CHECK_OK(InitRecordBatch(kHTTPElements, state.range(0), &record_batch));
  for (auto _ : state) {
    for (auto& element : record_batch) {
      element->Clear();
    }
    for (size_t i = 0; i < static_cast<size_t>(state.range(0)); i++) {
      ConsumeHTTPRecordRefl(record, &record_batch);
    }
  }
  state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(HTTPRecord));
}

BENCHMARK(BM_record_batch_pb)->RangeMultiplier(kRangeMultiplier)->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_record_batch_pb_refl)
    ->RangeMultiplier(kRangeMultiplier)
    ->Range(kRangeBegin, kRangeEnd);

}  // namespace px
