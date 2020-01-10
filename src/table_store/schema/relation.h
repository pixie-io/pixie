#pragma once

#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/table_store/proto/schema.pb.h"

namespace pl {
namespace table_store {
namespace schema {

using ColTypeArray = std::vector<types::DataType>;
using ColNameArray = std::vector<std::string>;
using ColDescArray = std::vector<std::string>;

/**
 * Relation tracks columns/types for a given table/operator
 */
class Relation {
 public:
  Relation();
  // Constructor for Relation that initializes with a list of column types.
  explicit Relation(ColTypeArray col_types, ColNameArray col_names);
  explicit Relation(ColTypeArray col_types, ColNameArray col_names, ColDescArray col_desc);

  // Get the column types.
  const ColTypeArray& col_types() const { return col_types_; }
  // Get the column names.
  const ColNameArray& col_names() const { return col_names_; }

  // Returns the number of columns.
  size_t NumColumns() const;

  // Add a column to the relation.
  void AddColumn(const types::DataType& col_type, const std::string& col_name,
                 std::string_view desc = "");

  int64_t GetColumnIndex(const std::string& col_name) const;

  // Check if the column at idx exists.
  bool HasColumn(size_t idx) const;
  bool HasColumn(const std::string& col_name) const;

  types::DataType GetColumnType(size_t idx) const;
  types::DataType GetColumnType(const std::string& col_name) const;
  const std::string& GetColumnName(size_t idx) const;
  const std::string& GetColumnDesc(size_t idx) const;

  // Get the debug string of this relation.
  std::string DebugString() const;

  /**
   * @brief Makes a new relation that has the specified columns.
   */
  StatusOr<Relation> MakeSubRelation(const std::vector<std::string>& columns) const;

  /**
   * Convert relation and write to passed in proto.
   * @param relation_proto The proto to write.
   * @return The status of conversion.
   */
  Status ToProto(table_store::schemapb::Relation* relation_proto) const;
  /**
   * @brief Initialize the Relation from a proto.
   * Will fail if columns already exist in the relation.
   *
   * @param relation_proto
   * @return Status
   */
  Status FromProto(const table_store::schemapb::Relation* relation_pb);

  bool operator==(const Relation& relation) const {
    return col_types() == relation.col_types() && col_names() == relation.col_names();
  }

  bool operator!=(const Relation& relation) const { return !(*this == relation); }

  friend std::ostream& operator<<(std::ostream& out, const Relation& relation) {
    return out << relation.DebugString();
  }

 private:
  ColTypeArray col_types_;
  ColNameArray col_names_;
  ColDescArray col_desc_;
};

}  // namespace schema
}  // namespace table_store
}  // namespace pl
