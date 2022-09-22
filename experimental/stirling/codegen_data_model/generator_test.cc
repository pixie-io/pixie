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

#include <gtest/gtest.h>

#include <vector>

#include "experimental/stirling/codegen_data_model/generator.h"
#include "experimental/stirling/codegen_data_model/http_record.h"
#include "src/shared/types/column_wrapper.h"
#include "src/stirling/types.h"

TEST(GeneratorTest, ProducesText) {
  EXPECT_EQ(R"(#pragma once

#include <string>
#include <utility>

#include "src/shared/types/column_wrapper.h"
#include "src/stirling/types.h"

namespace px {
namespace stirling {

// Auto generated struct for HTTPRecord.
struct HTTPRecord {
  int a;
};

// Auto generated function for writing HTTPRecord to record batch.
inline void ConsumeHTTPRecord(HTTPRecord record, types::ColumnWrapperRecordBatch* record_batch) {
  const uint64_t kExpectedElementCount = 1;
  CHECK(record_batch->size() == kExpectedElementCount)
      << absl::StrFormat("HTTP trace record field count should be: %d, got %d",
                         kExpectedElementCount, record_batch->size());
  auto& columns = *record_batch;

  uint32_t idx = 0;
  columns[idx++]->Append<types::Int64Value>(record.a);
}

// Auto generated function to get DataElement objects for HTTPRecord.
inline std::vector<DataElement> HTTPRecordElements() {
  return {
      DataElement("a", types::DataType::INT64),
  };
}

}  // namespace stirling
}  // namespace px
)",
            GenerateCode("HTTPRecord", R"(
    int a
  )"));
}

namespace px {
namespace stirling {

TEST(HTTPRecordTest, Normal) {
  HTTPRecord record{"test", 100};
  std::vector<DataElement> elements = HTTPRecordElements();
  types::ColumnWrapperRecordBatch record_batch;
  EXPECT_OK(InitRecordBatch(elements, 100, &record_batch));
  ConsumeHTTPRecord(record, &record_batch);
  EXPECT_STREQ("test", record_batch[0]->Get<types::StringValue>(0).c_str());
  EXPECT_EQ(100, record_batch[1]->Get<types::Int64Value>(0).val);
}

}  // namespace stirling
}  // namespace px
