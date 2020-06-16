#pragma once

#include <utility>
#include <vector>

#include "src/stirling/canonical_types.h"
#include "src/stirling/data_table.h"
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
