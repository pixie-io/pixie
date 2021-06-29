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

#include <utility>
#include <vector>

#include "src/stirling/core/canonical_types.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/types.h"

namespace px {
namespace stirling {
namespace testing {

// clang-format off
constexpr DataElement kDummyElements[] = {
    {"int64", "int64",
     types::DataType::INT64,
     types::SemanticType::ST_NONE,
     types::PatternType::METRIC_COUNTER},
    {"string", "string",
     types::DataType::STRING,
     types::SemanticType::ST_NONE,
     types::PatternType::STRUCTURED},
};
// clang-format on

constexpr auto kDummyTable = DataTableSchema("dummy", "A dummy table for testing", kDummyElements);
DEFINE_PRINT_TABLE(Dummy);

template <const DataTableSchema* schema>
struct TableFixture {
  TableFixture() : data_table(/*id*/ 0, *schema) {}

  types::ColumnWrapperRecordBatch record_batch() {
    std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
    // Tabletization not yet supported, so expect only one tablet.
    CHECK_EQ(tablets.size(), 1);
    return std::move(tablets.front().records);
  }
  ArrayView<DataElement> elements() const { return kDummyTable.elements(); }
  DataTable::RecordBuilder<schema> record_builder() {
    return DataTable::RecordBuilder<schema>(&data_table);
  }
  stirlingpb::TableSchema SchemaProto() { return schema->ToProto(); }

  DataTable data_table;
};

using DummyTableFixture = TableFixture<&kDummyTable>;

namespace dummy_table_idx {

constexpr int kInt64Idx = kDummyTable.ColIndex("int64");
constexpr int kStringIdx = kDummyTable.ColIndex("string");

}  // namespace dummy_table_idx

}  // namespace testing
}  // namespace stirling
}  // namespace px
