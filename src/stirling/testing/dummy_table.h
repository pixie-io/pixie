#pragma once

#include "src/stirling/canonical_types.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {
namespace testing {

// clang-format off
constexpr DataElement kDummyElements[] = {
    {"int64", types::DataType::INT64, types::PatternType::METRIC_COUNTER, "int64"},
    {"string", types::DataType::STRING, types::PatternType::STRUCTURED, "string"},
};
// clang-format on

constexpr auto kDummyTable = DataTableSchema("dummy", kDummyElements);

template <const DataTableSchema* schema>
struct TableFixture {
  TableFixture() : data_table(*schema) {}

  const types::ColumnWrapperRecordBatch& record_batch() { return *data_table.ActiveRecordBatch(); }
  ArrayView<DataElement> elements() const { return kDummyTable.elements(); }
  RecordBuilder<schema> record_builder() { return RecordBuilder<schema>(&data_table); }

  DataTable data_table;
};

using DummyTableFixture = TableFixture<&kDummyTable>;

namespace dummy_table_idx {

constexpr int kInt64Idx = kDummyTable.ColIndex("int64");
constexpr int kStringIdx = kDummyTable.ColIndex("string");

}  // namespace dummy_table_idx

}  // namespace testing
}  // namespace stirling
}  // namespace pl
