#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/table.h"
#include "src/carnot/exec/table_store.h"
#include "src/stirling/bpftrace_connector.h"

namespace pl {
namespace carnot {
namespace exec {

TEST(TableStoreTest, basic) {
  auto descriptor1 =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd1 = RowDescriptor(descriptor1);
  auto descriptor2 = std::vector<types::DataType>(
      {types::DataType::INT64, types::DataType::FLOAT64, types::DataType::INT64});
  RowDescriptor rd2 = RowDescriptor(descriptor2);

  auto table1 = std::make_shared<Table>(rd1);
  EXPECT_OK(table1->AddColumn(std::make_shared<Column>(types::DataType::BOOLEAN, "table1col1")));
  EXPECT_OK(table1->AddColumn(std::make_shared<Column>(types::DataType::FLOAT64, "table1col2")));
  auto table2 = std::make_shared<Table>(rd2);
  EXPECT_OK(table2->AddColumn(std::make_shared<Column>(types::DataType::INT64, "table2col1")));
  EXPECT_OK(table2->AddColumn(std::make_shared<Column>(types::DataType::FLOAT64, "table2col2")));
  EXPECT_OK(table2->AddColumn(std::make_shared<Column>(types::DataType::INT64, "table2col3")));

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

TEST(TableStoreTest, throwaway_default_table) {
  auto table_store = TableStore();
  table_store.AddDefaultTable();

  auto lookup = table_store.GetRelationMap();
  EXPECT_EQ(1, lookup->size());

  auto table = lookup->at(DefaultTableSchema::kName);
  EXPECT_EQ(DefaultTableSchema::kElements.size(), table.NumColumns());
  for (uint32_t i = 0; i < DefaultTableSchema::kElements.size(); ++i) {
    EXPECT_EQ(DefaultTableSchema::kElements[i].type(), table.GetColumnType(i));
    EXPECT_EQ(DefaultTableSchema::kElements[i].name(), table.GetColumnName(i));
  }
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
