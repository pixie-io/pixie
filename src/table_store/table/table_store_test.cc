/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/table/table_store.h"

namespace px {
namespace table_store {

using table_store::Table;
using table_store::schema::RowDescriptor;
using types::ColumnWrapperRecordBatch;

class TableStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rel1 = schema::Relation({types::DataType::BOOLEAN, types::DataType::FLOAT64},
                            {"table1col1", "table1col2"});
    rel2 =
        schema::Relation({types::DataType::INT64, types::DataType::FLOAT64, types::DataType::INT64},
                         {"table2col1", "table2col2", "table2col3"});
    table1 = Table::Create("test_table1", rel1);
    table2 = Table::Create("test_table2", rel2);
  }

  std::unique_ptr<ColumnWrapperRecordBatch> MakeRel1ColumnWrapperBatch() {
    std::vector<types::BoolValue> col1 = {true, true, false};
    std::vector<types::Float64Value> col2 = {1.1, 5.0, 2.9};
    auto wrapper_batch_1 = std::make_unique<types::ColumnWrapperRecordBatch>();
    auto col_wrapper_1 = std::make_shared<types::BoolValueColumnWrapper>(3);
    auto col_wrapper_2 = std::make_shared<types::Float64ValueColumnWrapper>(3);
    col_wrapper_1->Clear();
    col_wrapper_1->AppendFromVector(col1);
    col_wrapper_2->Clear();
    col_wrapper_2->AppendFromVector(col2);
    wrapper_batch_1->push_back(col_wrapper_1);
    wrapper_batch_1->push_back(col_wrapper_2);
    return wrapper_batch_1;
  }

  std::shared_ptr<Table> table1;
  std::shared_ptr<Table> table2;
  schema::Relation rel1;
  schema::Relation rel2;
};

TEST_F(TableStoreTest, basic) {
  auto table_store = TableStore();
  table_store.AddTable(table1, "a");
  table_store.AddTable(table2, "b");

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

TEST_F(TableStoreTest, get_table_ids) {
  auto table_store = TableStore();
  table_store.AddTable(table1, "a", 1);
  table_store.AddTable(table2, "b", 20);

  EXPECT_THAT(table_store.GetTableIDs(), ::testing::UnorderedElementsAre(1, 20));
}

TEST_F(TableStoreTest, table_id_aliasing) {
  auto table_store = TableStore();

  const uint64_t kTableID = 1;
  const uint64_t kAliasID = 5;

  table_store.AddTable(table1, "a", kTableID);
  EXPECT_OK(table_store.AddTableAlias(kAliasID, "a"));

  Table* table;

  table = table_store.GetTable("a");
  EXPECT_EQ(table->GetTableStats().bytes, 0);
  EXPECT_EQ(table->GetTableStats().batches_added, 0);

  table = table_store.GetTable(kTableID);
  EXPECT_EQ(table->GetTableStats().bytes, 0);
  EXPECT_EQ(table->GetTableStats().batches_added, 0);

  table = table_store.GetTable(kAliasID);
  EXPECT_EQ(table->GetTableStats().bytes, 0);
  EXPECT_EQ(table->GetTableStats().batches_added, 0);

  EXPECT_OK(table_store.AppendData(kTableID, "", MakeRel1ColumnWrapperBatch()));

  table = table_store.GetTable("a");
  EXPECT_EQ(table->GetTableStats().bytes, 27);
  EXPECT_EQ(table->GetTableStats().batches_added, 1);

  table = table_store.GetTable(kTableID);
  EXPECT_EQ(table->GetTableStats().bytes, 27);
  EXPECT_EQ(table->GetTableStats().batches_added, 1);

  table = table_store.GetTable(kAliasID);
  EXPECT_EQ(table->GetTableStats().bytes, 27);
  EXPECT_EQ(table->GetTableStats().batches_added, 1);

  EXPECT_OK(table_store.AppendData(kAliasID, "", MakeRel1ColumnWrapperBatch()));

  table = table_store.GetTable("a");
  EXPECT_EQ(table->GetTableStats().bytes, 54);
  EXPECT_EQ(table->GetTableStats().batches_added, 2);

  table = table_store.GetTable(kTableID);
  EXPECT_EQ(table->GetTableStats().bytes, 54);
  EXPECT_EQ(table->GetTableStats().batches_added, 2);

  table = table_store.GetTable(kAliasID);
  EXPECT_EQ(table->GetTableStats().bytes, 54);
  EXPECT_EQ(table->GetTableStats().batches_added, 2);
}

using TableStoreDeathTest = TableStoreTest;
TEST_F(TableStoreDeathTest, rewrite_fails) {
  auto table_store = TableStore();
  table_store.AddTable(table1, "a");

  auto lookup = table_store.GetRelationMap();
  EXPECT_EQ(1, lookup->size());
  EXPECT_EQ(table1->GetRelation(), lookup->at("a"));
  EXPECT_FALSE(table2->GetRelation() == lookup->at("a"));

  EXPECT_DEBUG_DEATH(table_store.AddTable(table2, "a"), "name_to_relation_map_iter->second == .*");
}

const char* kTableStoreProto = R"proto(
relation_map {
  key: "a"
  value {
    columns {
      column_name: "table1col1"
      column_type: BOOLEAN
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "table1col2"
      column_type: FLOAT64
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "b"
  value {
    columns {
      column_name: "table1col1"
      column_type: BOOLEAN
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "table1col2"
      column_type: FLOAT64
      column_semantic_type: ST_NONE
    }
  }
}
)proto";
TEST_F(TableStoreTest, to_proto) {
  auto table_store = TableStore();
  table_store.AddTable(table1, "a");
  table_store.AddTable(table1, "b");
  schemapb::Schema actual_schema;
  schemapb::Schema expected_schema;
  EXPECT_OK(table_store.SchemaAsProto(&actual_schema));
  VLOG(2) << actual_schema.DebugString();

  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kTableStoreProto, &expected_schema));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(actual_schema, expected_schema));
}

class TableStoreTabletsTest : public TableStoreTest {
 protected:
  void SetUp() override {
    TableStoreTest::SetUp();
    tablet1_1 = Table::Create("test_table1", rel1);
    tablet1_2 = Table::Create("test_table1", rel1);
    tablet2_1 = Table::Create("test_table2", rel2);
  }

  std::shared_ptr<Table> tablet1_1;
  std::shared_ptr<Table> tablet1_2;
  std::shared_ptr<Table> tablet2_1;
};

TEST_F(TableStoreTabletsTest, tablet_test) {
  auto table_store = TableStore();
  uint64_t table_id = 123;
  types::TabletID tablet1_id = "456";
  types::TabletID tablet2_id = "789";

  // Create the containing table and add the tablets to the table.
  table_store.AddTable(tablet1_1, "a", table_id, tablet1_id);
  table_store.AddTable(tablet1_2, "a", table_id, tablet2_id);

  Table* tablet1 = table_store.GetTable("a", tablet1_id);
  EXPECT_EQ(tablet1->GetTableStats().bytes, 0);
  EXPECT_EQ(tablet1->GetTableStats().batches_added, 0);

  Table* tablet2 = table_store.GetTable("a", tablet2_id);
  EXPECT_EQ(tablet2->GetTableStats().bytes, 0);
  EXPECT_EQ(tablet2->GetTableStats().batches_added, 0);
  EXPECT_NE(tablet1, tablet2);

  EXPECT_OK(table_store.AppendData(table_id, tablet1_id, MakeRel1ColumnWrapperBatch()));

  // Compare the properties of the tablets.

  // Tablet 1 should have new bytes and batches.
  EXPECT_EQ(tablet1->GetTableStats().bytes, 27);
  EXPECT_EQ(tablet1->GetTableStats().batches_added, 1);

  // Tablet 2 shouldn't change.
  EXPECT_EQ(tablet2->GetTableStats().bytes, 0);
  EXPECT_EQ(tablet2->GetTableStats().batches_added, 0);
}

// Test to make sure that appending data makes a tablet.
TEST_F(TableStoreTabletsTest, add_tablet_on_append_data) {
  auto table_store = TableStore();
  uint64_t table_id = 123;
  types::TabletID tablet1_id = "456";
  types::TabletID tablet2_id = "789";

  // Only add tablet 2.
  table_store.AddTable(tablet1_1, "a", table_id, tablet2_id);

  Table* tablet2 = table_store.GetTable("a", tablet2_id);
  EXPECT_EQ(tablet2->GetTableStats().bytes, 0);
  EXPECT_EQ(tablet2->GetTableStats().batches_added, 0);

  EXPECT_OK(table_store.AppendData(table_id, tablet1_id, MakeRel1ColumnWrapperBatch()));

  // Tablet 1 should have new bytes and batches.
  Table* tablet1 = table_store.GetTable("a", tablet1_id);
  EXPECT_EQ(tablet1->GetTableStats().bytes, 27);
  EXPECT_EQ(tablet1->GetTableStats().batches_added, 1);

  // Tablet 2 shouldn't change.
  EXPECT_EQ(tablet2->GetTableStats().bytes, 0);
  EXPECT_EQ(tablet2->GetTableStats().batches_added, 0);
}

using TableStoreTabletsDeathTest = TableStoreTabletsTest;
TEST_F(TableStoreTabletsDeathTest, tablet_test) {
  auto table_store = TableStore();
  uint64_t table_id = 123;
  types::TabletID tablet1_id = "456";
  types::TabletID tablet2_id = "789";
  types::TabletID tablet3_id = "654";

  table_store.AddTable(tablet1_1, "a", table_id, tablet1_id);
  table_store.AddTable(tablet1_2, "a", table_id, tablet2_id);
  EXPECT_DEBUG_DEATH(table_store.AddTable(tablet2_1, "a", table_id, tablet3_id), "");
}

}  // namespace table_store
}  // namespace px
