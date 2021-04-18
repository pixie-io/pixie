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

#include "src/carnot/planner/ir/time.h"

namespace px {
namespace carnot {
namespace planner {

StatusOr<int64_t> ParseDurationFmt(const StringIR* node, int64_t time_now) {
  auto int_or_s = StringToTimeInt(node->str());
  if (!int_or_s.ok()) {
    return int_or_s.status();
  }
  return int_or_s.ConsumeValueOrDie() + time_now;
}  // namespace planner

StatusOr<int64_t> ParseAbsFmt(const StringIR* node, const std::string& format) {
  absl::Time tm;
  std::string err_str;
  if (!absl::ParseTime(format, node->str(), &tm, &err_str)) {
    return node->CreateIRNodeError("Failed to parse time: '$0'", err_str);
  }
  int64_t time_ns = absl::ToUnixNanos(tm);
  return time_ns;
}

StatusOr<int64_t> ParseStringToTime(const StringIR* node, int64_t time_now) {
  auto time_or_s = ParseDurationFmt(node, time_now);
  std::vector<Status> bad_status;
  if (!time_or_s.ok()) {
    bad_status.push_back(time_or_s.status());
    time_or_s = ParseAbsFmt(node, kAbsTimeFormat);
  }
  if (!time_or_s.ok()) {
    bad_status.push_back(time_or_s.status());
    return MergeStatuses(bad_status);
  }
  return time_or_s;
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
