#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/data_table.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

using types::ColumnWrapper;
using types::DataType;

DataTable::DataTable(const DataTableSchema& schema) : table_schema_(schema) {}

void DataTable::InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr) {
  DCHECK(record_batch_ptr != nullptr);
  DCHECK(record_batch_ptr->empty());

  for (const auto& element : table_schema_.elements()) {
    pl::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(kTargetCapacity);                  \
  record_batch_ptr->push_back(col);
    PL_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
}

types::ColumnWrapperRecordBatch* DataTable::ActiveRecordBatch(types::TabletIDView tablet_id) {
  auto& tablet_ptr = tablets_[tablet_id];
  if (tablet_ptr == nullptr) {
    tablet_ptr = std::make_unique<types::ColumnWrapperRecordBatch>();
    InitBuffers(tablet_ptr.get());
  }
  return tablet_ptr.get();
}

std::vector<TaggedRecordBatch> DataTable::ConsumeRecordBatches() {
  std::vector<TaggedRecordBatch> tablets_out;

  //  // Make sure there is always at least one tablet, even if it is empty.
  //  if (tablets_.empty()) {
  //    tablets_[0] = std::make_unique<types::ColumnWrapperRecordBatch>();
  //    InitBuffers(tablets_[0].get());
  //  }

  for (auto& [tablet_id, tablet] : tablets_) {
    // Sort based on time_ column, if it exists.
    std::vector<size_t> sort_indexes;
    size_t time_col_idx = table_schema_.ColIndex("time_");
    if (time_col_idx != table_schema_.elements().size()) {
      auto& time_col = (*tablet)[time_col_idx];
      sort_indexes = time_col->SortedIndexes();
    }

    for (size_t j = 0; j < table_schema_.elements().size(); ++j) {
      auto col = (*tablet)[j];
      if (!sort_indexes.empty()) {
        col->Reorder(sort_indexes);
      }
      col->ShrinkToFit();
    }
    tablets_out.push_back(TaggedRecordBatch{tablet_id, std::move(tablet)});
  }
  tablets_.clear();

  return tablets_out;
}

}  // namespace stirling
}  // namespace pl
