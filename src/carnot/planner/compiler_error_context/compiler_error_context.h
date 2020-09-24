#pragma once
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"

namespace pl {
namespace carnot {
namespace planner {
compilerpb::CompilerErrorGroup LineColErrorPb(int64_t line, int64_t column,
                                              std::string_view message);

void AddLineColError(compilerpb::CompilerErrorGroup* error_group, int64_t line, int64_t column,
                     std::string_view message);

compilerpb::CompilerErrorGroup MergeGroups(
    const std::vector<compilerpb::CompilerErrorGroup>& groups);

Status MergeStatuses(const std::vector<Status>& statuses);

}  // namespace planner
}  // namespace carnot
}  // namespace pl
