#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/table/table_store.h"

namespace pl {
namespace table_store {

using table_store::Column;
using table_store::Table;
using table_store::schema::RowDescriptor;

class TableStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    table_store::schema::Relation rel1({types::DataType::BOOLEAN, types::DataType::FLOAT64},
                                       {"table1col1", "table1col2"});
    table_store::schema::Relation rel2(
        {types::DataType::INT64, types::DataType::FLOAT64, types::DataType::INT64},
        {"table2col1", "table2col2", "table2col3"});
    table1 = std::make_shared<Table>(rel1);
    table2 = std::make_shared<Table>(rel2);
  }
  std::shared_ptr<Table> table1;
  std::shared_ptr<Table> table2;
};

TEST_F(TableStoreTest, basic) {
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

TEST_F(TableStoreTest, rewrite) {
  auto table_store = TableStore();
  table_store.AddTable("a", table1);

  auto lookup = table_store.GetRelationMap();
  EXPECT_EQ(1, lookup->size());
  EXPECT_EQ(types::DataType::BOOLEAN, lookup->at("a").GetColumnType(0));
  EXPECT_EQ("table1col1", lookup->at("a").GetColumnName(0));
  EXPECT_EQ(types::DataType::FLOAT64, lookup->at("a").GetColumnType(1));
  EXPECT_EQ("table1col2", lookup->at("a").GetColumnName(1));

  table_store.AddTable("a", table2);

  lookup = table_store.GetRelationMap();
  EXPECT_EQ(1, lookup->size());
  EXPECT_EQ(types::DataType::INT64, lookup->at("a").GetColumnType(0));
  EXPECT_EQ("table2col1", lookup->at("a").GetColumnName(0));
  EXPECT_EQ(types::DataType::FLOAT64, lookup->at("a").GetColumnType(1));
  EXPECT_EQ("table2col2", lookup->at("a").GetColumnName(1));
  EXPECT_EQ(types::DataType::INT64, lookup->at("a").GetColumnType(2));
  EXPECT_EQ("table2col3", lookup->at("a").GetColumnName(2));
}
const char* kTableStoreProto = R"proto(
relation_map {
  key: "a"
  value {
    columns {
      column_name: "table1col1"
      column_type: BOOLEAN
    }
    columns {
      column_name: "table1col2"
      column_type: FLOAT64
    }
  }
}
relation_map {
  key: "b"
  value {
    columns {
      column_name: "table1col1"
      column_type: BOOLEAN
    }
    columns {
      column_name: "table1col2"
      column_type: FLOAT64
    }
  }
}
)proto";
TEST_F(TableStoreTest, to_proto) {
  auto table_store = TableStore();
  table_store.AddTable("a", table1);
  table_store.AddTable("b", table1);
  schemapb::Schema actual_schema;
  schemapb::Schema expected_schema;
  EXPECT_OK(table_store.SchemaAsProto(&actual_schema));
  VLOG(2) << actual_schema.DebugString();

  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kTableStoreProto, &expected_schema));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(actual_schema, expected_schema));
}

}  // namespace table_store
}  // namespace pl
