#pragma once

#include <string_view>

#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

void PrintRecordBatch(std::string_view prefix, const ArrayView<DataElement>& schema,
                      const types::ColumnWrapperRecordBatch& record_batch);

}  // namespace stirling
}  // namespace pl
