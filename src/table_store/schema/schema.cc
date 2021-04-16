#include <algorithm>
#include <vector>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/table_store/schema/schema.h"

namespace px {
namespace table_store {
namespace schema {

bool Schema::HasRelation(int64_t id) const { return relations_.find(id) != relations_.end(); }

std::vector<int64_t> Schema::GetIDs() const {
  std::vector<int64_t> ids(relations_.size());
  std::transform(relations_.begin(), relations_.end(), ids.begin(),
                 [](const auto& pair) { return pair.first; });
  return ids;
}

void Schema::AddRelation(int64_t id, const Relation& relation) {
  VLOG_IF(1, HasRelation(id)) << absl::StrFormat("WARNING: Relation %d already exists", id);
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

StatusOr<const Relation> Schema::GetRelation(int64_t id) const {
  if (!HasRelation(id)) {
    return error::NotFound("no such relation: $0", id);
  }
  return relations_.at(id);
}

Status Schema::ToProto(schemapb::Schema* schema,
                       const absl::flat_hash_map<std::string, schema::Relation>& relation_map) {
  CHECK(schema != nullptr);
  auto map = schema->mutable_relation_map();
  for (auto& [table_name, relation] : relation_map) {
    schemapb::Relation* relation_pb = &(*map)[table_name];
    PL_RETURN_IF_ERROR(relation.ToProto(relation_pb));
  }
  return Status::OK();
}

}  // namespace schema
}  // namespace table_store
}  // namespace px
