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

#include "src/common/base/base.h"

namespace experimental {

constexpr size_t kSizeTime = 8;
constexpr size_t kSizeUPID = 16;
constexpr size_t kSizeTracerole = 8;
constexpr size_t kSizeLatency = 8;
constexpr std::string_view remote_addr = "10.162.144.10";
constexpr std::string_view remote_port = "38288";

constexpr size_t kFixedColumnSize =
    kSizeTime + kSizeUPID + kSizeTracerole + kSizeLatency + remote_addr.size() + remote_port.size();

template <typename RecordType>
px::StatusOr<RecordType*> InitRecordPb();

template <typename RecordType>
px::StatusOr<RecordType*> InitRecordJson();

template <typename RecordType>
px::StatusOr<size_t> GetRecordSizeJson(const RecordType* record);

template <typename RecordType>
px::StatusOr<size_t> GetRecordSizePb(const RecordType* record) {
  return kFixedColumnSize + record->ByteSizeLong();
}
}  // namespace experimental
