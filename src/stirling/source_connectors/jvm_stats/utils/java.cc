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

#include "src/stirling/source_connectors/jvm_stats/utils/java.h"

#include <absl/strings/match.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/byte_utils.h"
#include "src/common/base/statusor.h"
#include "src/common/fs/fs_wrapper.h"
#include "src/common/system/proc_parser.h"
#include "src/common/system/proc_pid_path.h"
#include "src/common/system/uid.h"
#include "src/stirling/source_connectors/jvm_stats/utils/hsperfdata.h"
#include "src/stirling/utils/proc_path_tools.h"

namespace px {
namespace stirling {
namespace java {

using ::px::stirling::java::hsperf::ParseHsperfData;
using ::px::system::ProcParser;
using ::px::system::ProcPidRootPath;
using ::px::utils::LEndianBytesToInt;

Stats::Stats(std::vector<Stat> stats) : stats_(std::move(stats)) {}

Stats::Stats(std::string hsperf_data_str) : hsperf_data_(std::move(hsperf_data_str)) {}

Status Stats::Parse() {
  hsperf::HsperfData hsperf_data = {};
  PX_RETURN_IF_ERROR(ParseHsperfData(hsperf_data_, &hsperf_data));
  for (const auto& entry : hsperf_data.data_entries) {
    if (entry.header->data_type != static_cast<uint8_t>(hsperf::DataType::kLong)) {
      continue;
    }
    auto value = LEndianBytesToInt<uint64_t>(entry.data);
    stats_.push_back({entry.name, value});
  }
  return Status::OK();
}

uint64_t Stats::YoungGCTimeNanos() const { return StatForSuffix("gc.collector.0.time"); }

uint64_t Stats::FullGCTimeNanos() const { return StatForSuffix("gc.collector.1.time"); }

uint64_t Stats::UsedHeapSizeBytes() const {
  return SumStatsForSuffixes({
      "gc.generation.0.space.0.used",
      "gc.generation.0.space.1.used",
      "gc.generation.0.space.2.used",
      "gc.generation.1.space.0.used",
  });
}

uint64_t Stats::TotalHeapSizeBytes() const {
  return SumStatsForSuffixes({
      "gc.generation.0.space.0.capacity",
      "gc.generation.0.space.1.capacity",
      "gc.generation.0.space.2.capacity",
      "gc.generation.1.space.0.capacity",
  });
}

uint64_t Stats::MaxHeapSizeBytes() const {
  return SumStatsForSuffixes({
      "gc.generation.0.maxCapacity",
      "gc.generation.1.maxCapacity",
  });
}

uint64_t Stats::StatForSuffix(std::string_view suffix) const {
  for (const auto& stat : stats_) {
    if (absl::EndsWith(stat.name, suffix)) {
      return stat.value;
    }
  }
  return 0;
}

uint64_t Stats::SumStatsForSuffixes(const std::vector<std::string_view>& suffixes) const {
  uint64_t sum = 0;
  for (const auto& suffix : suffixes) {
    sum += StatForSuffix(suffix);
  }
  return sum;
}

StatusOr<std::filesystem::path> HsperfdataPath(pid_t pid) {
  ProcParser parser;

  ProcParser::ProcUIDs uids;
  PX_RETURN_IF_ERROR(parser.ReadUIDs(pid, &uids));

  uid_t effective_uid = 0;
  if (!absl::SimpleAtoi(uids.effective, &effective_uid)) {
    return error::InvalidArgument("Invalid uid: '$0'", uids.effective);
  }

  const std::filesystem::path passwd_path = ProcPidRootPath(pid, "etc", "passwd");

  PX_ASSIGN_OR_RETURN(const std::string passwd_content, ReadFileToString(passwd_path));
  std::map<uid_t, std::string> uid_user_map = ParsePasswd(passwd_content);
  auto iter = uid_user_map.find(effective_uid);
  if (iter == uid_user_map.end()) {
    return error::InvalidArgument("PID=$0 effective_uid=$1 cannot be found in $2", pid,
                                  effective_uid, passwd_path.string());
  }
  const std::string& effective_user = iter->second;

  std::vector<std::string> ns_pids;
  PX_RETURN_IF_ERROR(parser.ReadNSPid(pid, &ns_pids));
  // The right-most pid is the PID of the same process inside the most-nested namespace.
  // That will be the filename chosen by the running process.
  const std::string& ns_pid = ns_pids.back();
  const std::string hsperf_user = absl::Substitute("hsperfdata_$0", effective_user);
  const auto hsperf_data_path = ProcPidRootPath(pid, "tmp", hsperf_user, ns_pid);
  if (!fs::Exists(hsperf_data_path)) {
    return error::NotFound("Could find hsperf data file path: $0.", hsperf_data_path.string());
  }
  return hsperf_data_path;
}

}  // namespace java
}  // namespace stirling
}  // namespace px
