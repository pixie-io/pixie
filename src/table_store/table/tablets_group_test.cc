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

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/table_store/schema/relation.h"
#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/table/table_store.h"

namespace px {
namespace table_store {

using table_store::Table;
using table_store::schema::RowDescriptor;
using types::ColumnWrapperRecordBatch;

class TabletsGroupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rel1 = schema::Relation({types::DataType::BOOLEAN, types::DataType::FLOAT64},
                            {"table1col1", "table1col2"});
    rel2 =
        schema::Relation({types::DataType::INT64, types::DataType::FLOAT64, types::DataType::INT64},
                         {"table2col1", "table2col2", "table2col3"});
    tablet1 = Table::Create("test_table1", rel1);
    tablet2 = Table::Create("test_table2", rel2);
  }

  std::shared_ptr<Table> tablet1;
  std::shared_ptr<Table> tablet2;
  schema::Relation rel1;
  schema::Relation rel2;
};

TEST_F(TabletsGroupTest, TabletIdNotFoundMissing) {
  types::TabletID tablet_id1 = "123";
  types::TabletID tablet_id2 = "456";

  auto table = TabletsGroup(rel1);
  // Only add tablet_id1.
  table.AddTablet(tablet_id1, tablet1);

  EXPECT_NE(table.GetTablet(tablet_id1), nullptr);
  EXPECT_EQ(table.GetTablet(tablet_id2), nullptr);
}

}  // namespace table_store
}  // namespace px
