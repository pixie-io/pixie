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
#include <utility>
#include <vector>

#include <absl/strings/str_format.h>
#include "../../shared/types/types.h"

namespace px {
namespace table_store {
namespace schema {

/**
 * RowDescriptor describes the datatypes for each column in a RowBatch.
 */
class RowDescriptor {
 public:
  explicit RowDescriptor(std::vector<types::DataType> types) : types_(std::move(types)) {}

  /**
   * Gets all the datatypes in the row descriptor.
   * @ return Vector of datatypes.
   */
  const std::vector<types::DataType>& types() const { return types_; }
  std::vector<types::DataType>& types() { return types_; }

  /**
   *  Gets the datatype for a specific column index.
   *  @ return the UDFDataType for the given column index.
   */
  types::DataType type(int64_t i) const { return types_[i]; }

  /**
   * @ return the number of columns that the row descriptor is describing.
   */
  size_t size() const { return types_.size(); }

  /**
   * @return the debug string for the row descriptor.
   */
  std::string DebugString() const {
    std::string debug_string = "RowDescriptor:\n";
    for (const auto& type : types_) {
      debug_string += absl::StrFormat("  %d\n", type);
    }
    return debug_string;
  }

  bool operator==(const RowDescriptor& other) const { return other.types_ == types_; }

 private:
  std::vector<types::DataType> types_;
};

}  // namespace schema
}  // namespace table_store
}  // namespace px
