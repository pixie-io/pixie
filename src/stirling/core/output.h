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

#include <string>
#include <string_view>

#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/stirling/core/types.h"

namespace px {
namespace stirling {

// Returns a string representation of the row specified by index.
std::string ToString(const stirlingpb::TableSchema& schema,
                     const types::ColumnWrapperRecordBatch& record_batch, size_t index);

std::string ToString(std::string_view prefix, const stirlingpb::TableSchema& schema,
                     const types::ColumnWrapperRecordBatch& record_batch);

void PrintRecordBatch(std::string_view prefix, const stirlingpb::TableSchema& schema,
                      const types::ColumnWrapperRecordBatch& record_batch);

}  // namespace stirling
}  // namespace px
