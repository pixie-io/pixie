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

// DO NOT EDIT! Generated from experimental/stirling/codegen_data_model/HTTPRecord
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "src/shared/types/column_wrapper.h"
#include "src/stirling/types.h"

namespace px {
namespace stirling {

// Auto generated struct for HTTPRecord.
struct HTTPRecord {
  std::string a;
  int b;
};

// Auto generated function for writing HTTPRecord to record batch.
inline void ConsumeHTTPRecord(HTTPRecord record, types::ColumnWrapperRecordBatch* record_batch) {
  const uint64_t kExpectedElementCount = 2;
  CHECK(record_batch->size() == kExpectedElementCount)
      << absl::StrFormat("HTTP trace record field count should be: %d, got %d",
                         kExpectedElementCount, record_batch->size());
  auto& columns = *record_batch;

  uint32_t idx = 0;
  columns[idx++]->Append<types::StringValue>(std::move(record.a));
  columns[idx++]->Append<types::Int64Value>(record.b);
}

// Auto generated function to get DataElement objects for HTTPRecord.
inline std::vector<DataElement> HTTPRecordElements() {
  return {
      DataElement("a", types::DataType::STRING, types::PatternType::UNSPECIFIED, ""),
      DataElement("b", types::DataType::INT64, types::PatternType::UNSPECIFIED, ""),
  };
}

}  // namespace stirling
}  // namespace px
