#include <glog/logging.h>
#include <algorithm>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/plan/schema.h"

namespace pl {
namespace carnot {
namespace plan {

bool plan::Schema::HasRelation(int64_t id) const { return relations_.find(id) != relations_.end(); }

std::vector<int64_t> Schema::GetIDs() const {
  std::vector<int64_t> ids(relations_.size());
  std::transform(relations_.begin(), relations_.end(), ids.begin(),
                 [](const auto& pair) { return pair.first; });
  return ids;
}

void Schema::AddRelation(int64_t id, const Relation& relation) {
  VLOG_IF(1, HasRelation(id)) << "WARNING: Relation already exists";
  relations_[id] = relation;
}

std::string Schema::DebugString() const {
  if (relations_.empty()) {
    return "Relation: <empty>";
  }
  std::string debug_string = "Relation:\n";
  for (const auto& pair : relations_) {
    debug_string += absl::StrFormat("  {%d} : %s\n", pair.first, pair.second.DebugString());
  }
  return debug_string;
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
