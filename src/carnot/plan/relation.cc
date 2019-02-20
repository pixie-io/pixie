#include <glog/logging.h>
#include <algorithm>
#include <string>

#include "absl/strings/str_join.h"

#include "src/carnot/plan/relation.h"
#include "src/carnot/plan/utils.h"

namespace pl {
namespace carnot {
namespace plan {

using std::string;

Relation::Relation() {}

Relation::Relation(const ColTypeArray &col_types, const ColNameArray &col_names)
    : col_types_(col_types), col_names_(col_names) {
  CHECK(col_types_.size() == col_names_.size()) << "Initialized with mismatched col names/sizes";
}

size_t Relation::NumColumns() const { return col_types_.size(); }

void Relation::AddColumn(const types::DataType &col_type, const std::string &col_name) {
  col_types_.push_back(col_type);
  col_names_.push_back(col_name);
}

bool Relation::HasColumn(size_t idx) const { return idx < col_types_.size(); }
int64_t Relation::GetColumnIndex(const std::string &col_name) const {
  auto it = std::find(col_names_.begin(), col_names_.end(), col_name);
  if (it == col_names_.end()) {
    return -1;
  }
  auto col_idx = std::distance(col_names_.begin(), it);
  return col_idx;
}

bool Relation::HasColumn(const std::string &col_name) const {
  return HasColumn(GetColumnIndex(col_name));
}

types::DataType Relation::GetColumnType(size_t idx) const {
  CHECK(HasColumn(idx)) << "Column does not exist";
  return col_types_[idx];
}

types::DataType Relation::GetColumnType(const std::string &col_name) const {
  return GetColumnType(GetColumnIndex(col_name));
}
std::string Relation::GetColumnName(size_t idx) const {
  CHECK(HasColumn(idx)) << "Column does not exist";
  return col_names_[idx];
}

std::string Relation::DebugString() const {
  CHECK(col_types_.size() == col_names_.size()) << "Mismatched col names/sizes";
  std::vector<string> col_info_as_str;
  for (size_t i = 0; i < col_types_.size(); ++i) {
    col_info_as_str.push_back(col_names_[i] + ":" + ToString(col_types_[i]));
  }
  return "[" + absl::StrJoin(col_info_as_str, ", ") + "]";
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
