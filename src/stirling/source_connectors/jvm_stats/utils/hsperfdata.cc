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

#include "src/stirling/source_connectors/jvm_stats/utils/hsperfdata.h"

#include <arpa/inet.h>

#include <absl/strings/substitute.h>
#include <algorithm>
#include <utility>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace java {
namespace hsperf {

constexpr uint8_t kExpectedMagic[] = {0xCA, 0xFE, 0xC0, 0xC0};

namespace {

Status ParseDataEntry(std::string_view buf_view, DataEntry* entry) {
  entry->header = reinterpret_cast<const DataEntryHeader*>(buf_view.data());
  if (entry->header->entry_length > buf_view.size()) {
    return error::InvalidArgument("Not enough data");
  }

  size_t name_end = buf_view.find('\0', entry->header->name_offset);
  if (name_end == std::string_view::npos) {
    entry->name = buf_view.substr(entry->header->name_offset);
  } else {
    entry->name =
        buf_view.substr(entry->header->name_offset, name_end - entry->header->name_offset);
  }
  // Only supports limited data types. The full list can be found in valueAsString() of
  // jdk.hotspot.agent/share/classes/sun/jvm/hotspot/runtime/PerfDataEntry.java
  if (entry->header->vector_length == 0) {
    if (entry->header->data_type == static_cast<uint8_t>(DataType::kLong)) {
      constexpr int kLongByteSize = 8;
      entry->data = buf_view.substr(entry->header->data_offset, kLongByteSize);
    } else {
      return error::InvalidArgument("Invalid data type for scalar data");
    }
  } else {
    if (entry->header->data_type == static_cast<uint8_t>(DataType::kByte) &&
        entry->header->data_units == static_cast<uint8_t>(DataUnits::kString) &&
        (entry->header->data_variability == static_cast<uint8_t>(DataVariability::kVariable) ||
         entry->header->data_variability == static_cast<uint8_t>(DataVariability::kConstant))) {
      entry->data = buf_view.substr(entry->header->data_offset, entry->header->vector_length);
    } else {
      return error::InvalidArgument("Invalid data type for array data");
    }
  }
  return Status::OK();
}

}  // namespace

Status ParseHsperfData(std::string_view buf_view, HsperfData* data) {
  if (buf_view.size() < sizeof(kExpectedMagic)) {
    return error::InvalidArgument("Not enough data");
  }
  if (*(reinterpret_cast<const uint32_t*>(buf_view.data())) !=
      *(reinterpret_cast<const uint32_t*>(kExpectedMagic))) {
    return error::InvalidArgument("Invalid magic");
  }

  data->prologue = reinterpret_cast<const Prologue*>(buf_view.data());

  if (data->prologue->entry_offset > buf_view.size()) {
    return error::InvalidArgument("Entry offset $0 is beyond the buffer size $1",
                                  data->prologue->entry_offset, buf_view.size());
  }

  buf_view.remove_prefix(data->prologue->entry_offset);

  for (size_t i = 0; i < data->prologue->num_entries; ++i) {
    if (buf_view.size() < sizeof(DataEntryHeader)) {
      break;
    }
    DataEntry entry = {};
    auto status = ParseDataEntry(buf_view, &entry);
    buf_view.remove_prefix(
        std::min(static_cast<size_t>(entry.header->entry_length), buf_view.size()));
    if (!status.ok()) {
      continue;
    }
    data->data_entries.push_back(std::move(entry));
  }
  return Status::OK();
}

}  // namespace hsperf
}  // namespace java
}  // namespace stirling
}  // namespace px
