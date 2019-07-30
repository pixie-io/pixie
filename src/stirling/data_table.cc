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

DataTable::DataTable(const InfoClassSchema& schema) {
  DCHECK_EQ(table_schema_.size(), 0ULL);
  for (const auto& info_class_element : schema) {
    table_schema_.emplace_back(info_class_element);
  }
}

DataTable::DataTable(const DataTableSchema& schema) : DataTable(InfoClassSchema(schema)) {}

void DataTable::InitBuffers(types::ColumnWrapperRecordBatch* record_batch_ptr) {
  DCHECK(record_batch_ptr != nullptr);
  DCHECK(record_batch_ptr->empty());

  for (const auto& element : table_schema_) {
    pl::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(kTargetCapacity);                  \
  record_batch_ptr->push_back(col);
    PL_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
}

types::ColumnWrapperRecordBatch* DataTable::ActiveRecordBatch(size_t tablet_id) {
  auto& tablet_ptr = tablets_[tablet_id];
  if (tablet_ptr == nullptr) {
    tablet_ptr = std::make_unique<types::ColumnWrapperRecordBatch>();
    InitBuffers(tablet_ptr.get());
  }
  return tablet_ptr.get();
}

std::vector<TaggedRecordBatch> DataTable::ConsumeRecordBatches() {
  SealActiveRecordBatch();

  std::vector<TaggedRecordBatch> return_batches;
  return_batches = std::move(sealed_batches_);
  sealed_batches_.clear();
  return return_batches;
}

void DataTable::SealActiveRecordBatch() {
  for (auto& [tablet_id, tablet] : tablets_) {
    PL_UNUSED(tablet_id);
    for (size_t j = 0; j < table_schema_.size(); ++j) {
      auto col = (*tablet)[j];
      col->ShrinkToFit();
    }
    sealed_batches_.push_back(TaggedRecordBatch{tablet_id, std::move(tablet)});
  }
  tablets_.clear();
}

}  // namespace stirling
}  // namespace pl
