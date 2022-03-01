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

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/stirling/core/types.h"
#include "src/stirling/proto/stirling.pb.h"

namespace px {
namespace stirling {

// Returns a list of string representations of all of the records in the record batch.
std::vector<std::string> ToString(const stirlingpb::TableSchema& schema,
                                  const types::ColumnWrapperRecordBatch& record_batch);

// Returns a string representation of all of the records in the record batch.
// Each line of record is prepended with the prefix.
std::string ToString(std::string_view prefix, const stirlingpb::TableSchema& schema,
                     const types::ColumnWrapperRecordBatch& record_batch);

std::string PrintRecords(const DataTableSchema& data_table_schema,
                         const types::ColumnWrapperRecordBatch& record_batch);

#define DEFINE_PRINT_TABLE(protocol_name)                       \
  inline std::string Print##protocol_name##Table(               \
      const types::ColumnWrapperRecordBatch& record_batch) {    \
    return PrintRecords(k##protocol_name##Table, record_batch); \
  }

}  // namespace stirling
}  // namespace px
