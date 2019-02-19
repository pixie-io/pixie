#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/exec/table.h"
#include "src/carnot/exec/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

TEST(TableStoreTest, basic) {
  auto descriptor1 =
      std::vector<udf::UDFDataType>({types::DataType::BOOLEAN, types::DataType::FLOAT64});
  RowDescriptor rd1 = RowDescriptor(descriptor1);
  auto descriptor2 = std::vector<udf::UDFDataType>(
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

  auto lookup = table_store.GetTableTypesLookup();
  EXPECT_EQ(2, lookup.size());
  EXPECT_EQ(types::DataType::BOOLEAN, lookup.at("a").at("table1col1"));
  EXPECT_EQ(types::DataType::FLOAT64, lookup.at("a").at("table1col2"));
  EXPECT_EQ(types::DataType::INT64, lookup.at("b").at("table2col1"));
  EXPECT_EQ(types::DataType::FLOAT64, lookup.at("b").at("table2col2"));
  EXPECT_EQ(types::DataType::INT64, lookup.at("b").at("table2col3"));
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
