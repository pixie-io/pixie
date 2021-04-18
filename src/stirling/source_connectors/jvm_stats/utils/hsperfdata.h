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

#include <cstdint>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/utils/parse_state.h"

namespace px {
namespace stirling {
namespace java {
namespace hsperf {

// src/java.management/share/classes/sun/management/counter/perf/Prologue.java
// This prologue is at the start of the hsperfdata file. Followed by data entries, whose number is
// specified in num_entries.
struct Prologue {
  const uint8_t magic[4];
  uint8_t byte_order;
  uint8_t major_version;
  uint8_t minor_version;
  uint8_t accessible;
  uint32_t used;
  uint32_t overflow;
  uint64_t mod_timestamp;
  uint32_t entry_offset;
  uint32_t num_entries;
};

// src/java.management/share/classes/sun/management/counter/perf/PerfDataEntry.java
// This header is at the start of each data entry. It specifies the offset of the name and data.
struct DataEntryHeader {
  uint32_t entry_length;
  uint32_t name_offset;
  uint32_t vector_length;
  uint8_t data_type;
  uint8_t flags;
  uint8_t data_units;
  uint8_t data_variability;
  uint32_t data_offset;
};

struct DataEntry {
  const DataEntryHeader* header;
  std::string_view name;
  std::string_view data;
  ParseState parse_state = ParseState::kSuccess;
};

enum class DataType : uint8_t {
  kUnknown = 0x0,
  kByte = 'B',
  kChar = 'C',
  kDouble = 'D',
  kFloat = 'F',
  kInt = 'I',
  kLong = 'J',
  kShort = 'S',
  kBool = 'Z',
  kVoid = 'V',
  kObject = 'L',
  kArray = '[',
};

enum class DataUnits : uint8_t {
  kString = 5,
};

enum class DataVariability : uint8_t {
  kConstant = 1,
  kVariable = 3,
};

struct HsperfData {
  const Prologue* prologue;
  std::vector<DataEntry> data_entries;
};

/**
 * Parses the input buffer, and writes the data fields. Returns OK if succeeded.
 */
Status ParseHsperfData(std::string_view buf_view, HsperfData* data);

}  // namespace hsperf
}  // namespace java
}  // namespace stirling
}  // namespace px
