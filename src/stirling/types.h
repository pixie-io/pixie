#pragma once

#include <arrow/type.h>
#include <memory>
#include <vector>

#include "src/carnot/udf/column_wrapper.h"

namespace pl {
namespace stirling {

using ArrowArrayBuilderUPtrVec = std::vector<std::unique_ptr<arrow::ArrayBuilder>>;
using ArrowRecordBatchSPtrVec = std::vector<std::shared_ptr<arrow::RecordBatch>>;

using ColumnWrapperRecordBatch = std::vector<carnot::udf::SharedColumnWrapper>;
using ColumnWrapperRecordBatchVec = std::vector<std::unique_ptr<ColumnWrapperRecordBatch>>;

using PushDataCallback = std::function<void(uint64_t, std::unique_ptr<ColumnWrapperRecordBatch>)>;

}  // namespace stirling
}  // namespace pl
