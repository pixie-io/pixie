#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/table_store.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

using table_store::schema::Column;
using table_store::schema::RowDescriptor;
using table_store::schema::Table;

TEST(TableStoreTest, basic) {
  table_store::schema::Relation rel1({types::DataType::BOOLEAN, types::DataType::FLOAT64},
                                     {"table1col1", "table1col2"});
  table_store::schema::Relation rel2(
      {types::DataType::INT64, types::DataType::FLOAT64, types::DataType::INT64},
      {"table2col1", "table2col2", "table2col3"});
  auto table1 = std::make_shared<Table>(rel1);
  auto table2 = std::make_shared<Table>(rel2);

  auto table_store = TableStore();
  table_store.AddTable("a", table1);
  table_store.AddTable("b", table2);

  auto lookup = table_store.GetRelationMap();
  EXPECT_EQ(2, lookup->size());
  EXPECT_EQ(types::DataType::BOOLEAN, lookup->at("a").GetColumnType(0));
  EXPECT_EQ("table1col1", lookup->at("a").GetColumnName(0));
  EXPECT_EQ(types::DataType::FLOAT64, lookup->at("a").GetColumnType(1));
  EXPECT_EQ("table1col2", lookup->at("a").GetColumnName(1));
  EXPECT_EQ(types::DataType::INT64, lookup->at("b").GetColumnType(0));
  EXPECT_EQ("table2col1", lookup->at("b").GetColumnName(0));
  EXPECT_EQ(types::DataType::FLOAT64, lookup->at("b").GetColumnType(1));
  EXPECT_EQ("table2col2", lookup->at("b").GetColumnName(1));
  EXPECT_EQ(types::DataType::INT64, lookup->at("b").GetColumnType(2));
  EXPECT_EQ("table2col3", lookup->at("b").GetColumnName(2));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
