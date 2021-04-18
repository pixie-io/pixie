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
#include <gtest/gtest.h>

#include "src/carnot/planner/types/types.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/relation.h"

namespace px {
namespace carnot {
namespace planner {

TEST(ValueType, debug_string) {
  auto type = ValueType::Create(types::STRING, types::ST_SERVICE_NAME);
  EXPECT_EQ("ValueType(STRING, ST_SERVICE_NAME)", type->DebugString());
}

TEST(TableType, basic) {
  auto table = TableType::Create();
  table->AddColumn("col1", ValueType::Create(types::STRING, types::ST_SERVICE_NAME));
  table->AddColumn("col2", ValueType::Create(types::UINT128, types::ST_UPID));
  EXPECT_TRUE(table->HasColumn("col1"));
  EXPECT_TRUE(table->HasColumn("col2"));

  EXPECT_TRUE(table->RemoveColumn("col1"));

  EXPECT_FALSE(table->HasColumn("col1"));
  EXPECT_TRUE(table->HasColumn("col2"));

  EXPECT_TRUE(table->RenameColumn("col2", "my_new_name"));

  EXPECT_FALSE(table->HasColumn("col2"));
  EXPECT_TRUE(table->HasColumn("my_new_name"));
}

TEST(TableType, from_relation) {
  auto rel = Relation({types::STRING, types::FLOAT64}, {"col1", "col2"},
                      {types::ST_SERVICE_NAME, types::ST_NONE});
  auto table = TableType::Create(rel);
  ASSERT_TRUE(table->HasColumn("col1"));
  ASSERT_TRUE(table->HasColumn("col2"));
  auto col1_type = table->GetColumnType("col1").ValueOrDie();
  auto col2_type = table->GetColumnType("col2").ValueOrDie();
  EXPECT_EQ(*ValueType::Create(types::STRING, types::ST_SERVICE_NAME),
            *std::static_pointer_cast<ValueType>(col1_type));
  EXPECT_EQ(*ValueType::Create(types::FLOAT64, types::ST_NONE),
            *std::static_pointer_cast<ValueType>(col2_type));
}

TEST(TableType, to_relation) {
  auto rel = Relation({types::INT64, types::STRING}, {"col1", "col2"},
                      {types::ST_NONE, types::ST_POD_NAME});
  auto table = TableType::Create(rel);
  auto rel2 = table->ToRelation().ValueOrDie();
  EXPECT_EQ(rel, rel2);
}

TEST(TableType, debug_string) {
  auto table = TableType::Create();
  table->AddColumn("col1", ValueType::Create(types::STRING, types::ST_SERVICE_NAME));
  table->AddColumn("col2", ValueType::Create(types::UINT128, types::ST_UPID));

  EXPECT_EQ(
      "TableType(col1: ValueType(STRING, ST_SERVICE_NAME) | col2: ValueType(UINT128, ST_UPID))",
      table->DebugString());
}

TEST(TableType, iterator) {
  auto table = TableType::Create();
  table->AddColumn("col1", ValueType::Create(types::STRING, types::ST_SERVICE_NAME));
  table->AddColumn("col2", ValueType::Create(types::INT64, types::ST_NONE));
  table->AddColumn("col3", ValueType::Create(types::UINT128, types::ST_UPID));
  EXPECT_TRUE(table->RemoveColumn("col2"));
  EXPECT_TRUE(table->RenameColumn("col3", "aaa"));

  std::vector<std::pair<const std::string, TypePtr>> columns = {table->begin(), table->end()};

  EXPECT_EQ(2, columns.size());
  EXPECT_EQ("col1", columns[0].first);
  EXPECT_EQ("aaa", columns[1].first);
  EXPECT_EQ(*ValueType::Create(types::STRING, types::ST_SERVICE_NAME),
            *std::static_pointer_cast<ValueType>(columns[0].second));
  EXPECT_EQ(*ValueType::Create(types::UINT128, types::ST_UPID),
            *std::static_pointer_cast<ValueType>(columns[1].second));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
