#include "src/carnot/compiler/compiler_error_context.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pl {
namespace carnot {
namespace compiler {
compilerpb::CompilerErrorGroup LineColErrorPb(int64_t line, int64_t column, std::string message) {
  compilerpb::CompilerErrorGroup error_group;
  compilerpb::CompilerError* err = error_group.add_errors();
  compilerpb::LineColError* lc_err_pb = err->mutable_line_col_error();
  lc_err_pb->set_line(line);
  lc_err_pb->set_column(column);
  lc_err_pb->set_message(message);
  return error_group;
}

compilerpb::CompilerErrorGroup MergeGroups(
    const std::vector<compilerpb::CompilerErrorGroup>& groups) {
  compilerpb::CompilerErrorGroup out_error_group;
  for (const compilerpb::CompilerErrorGroup& group : groups) {
    for (const auto& error : group.errors()) {
      *(out_error_group.add_errors()) = error;
    }
  }
  return out_error_group;
}

Status MergeStatuses(const std::vector<Status>& statuses) {
  CHECK_NE(statuses.size(), 0UL);
  std::vector<compilerpb::CompilerErrorGroup> error_group_pbs;
  for (const auto s : statuses) {
    if (!s.has_context() || !s.context()->Is<compilerpb::CompilerErrorGroup>()) {
      continue;
    }
    compilerpb::CompilerErrorGroup cur_error;
    s.context()->UnpackTo(&cur_error);
    error_group_pbs.push_back(cur_error);
  }
  return Status(statuses[0].code(), statuses[0].msg(),
                std::make_unique<compilerpb::CompilerErrorGroup>(MergeGroups(error_group_pbs)));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
