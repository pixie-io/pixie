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

#include <absl/strings/substitute.h>
#include <magic_enum.hpp>

#include "src/stirling/utils/monitor.h"

namespace px {
namespace stirling {

inline bool operator==(const SourceStatusRecord& a, const SourceStatusRecord& b) {
  return (a.source_connector == b.source_connector) && (a.status == b.status) &&
         (a.error == b.error) && (a.context == b.context);
}

inline void PrintTo(const SourceStatusRecord& r, std::ostream* os) {
  *os << absl::Substitute(
      "SourceStatusRecord{timestamp_ns: $0, source_connector: $1, status: $2, error: $3, "
      "context: $4}",
      r.timestamp_ns, r.source_connector, magic_enum::enum_name(r.status), r.error, r.context);
}

inline bool operator==(const ProbeStatusRecord& a, const ProbeStatusRecord& b) {
  return (a.source_connector == b.source_connector) && (a.tracepoint == b.tracepoint) &&
         (a.status == b.status) && (a.error == b.error) && (a.info == b.info);
}

inline void PrintTo(const ProbeStatusRecord& r, std::ostream* os) {
  *os << absl::Substitute(
      "ProbeStatusRecord{timestamp_ns: $0, source_connector: $1, tracepoint: $2, status: "
      "$3, error: $4, "
      "info: $5}",
      r.timestamp_ns, r.source_connector, r.tracepoint, magic_enum::enum_name(r.status), r.error,
      r.info);
}

}  // namespace stirling
}  // namespace px
