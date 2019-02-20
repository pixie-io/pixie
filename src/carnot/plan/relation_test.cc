#include <gtest/gtest.h>

#include "src/carnot/plan/relation.h"

namespace pl {
namespace carnot {
namespace plan {

TEST(RelationTest, empty_relation) {
  Relation r;
  EXPECT_EQ(0, r.NumColumns());
  EXPECT_EQ("[]", r.DebugString());
  EXPECT_EQ(ColTypeArray({}), r.col_types());
  EXPECT_EQ(ColNameArray({}), r.col_names());
  EXPECT_FALSE(r.HasColumn(0));
}

TEST(RelationTest, basic_tests) {
  Relation r({types::INT64, types::STRING}, {"abc", "def"});
  EXPECT_EQ(2, r.NumColumns());
  EXPECT_EQ("[abc:int64, def:string]", r.DebugString());
  EXPECT_EQ(ColTypeArray({types::INT64, types::STRING}), r.col_types());
  EXPECT_TRUE(r.HasColumn(0));
  EXPECT_TRUE(r.HasColumn(1));
  EXPECT_FALSE(r.HasColumn(2));
  EXPECT_EQ(0, r.GetColumnIndex("abc"));
  EXPECT_EQ(1, r.GetColumnIndex("def"));
  EXPECT_TRUE(r.HasColumn("abc"));
  EXPECT_TRUE(r.HasColumn("def"));
  EXPECT_FALSE(r.HasColumn("abcde"));
  EXPECT_EQ(r.GetColumnType("abc"), types::INT64);
  EXPECT_EQ(r.GetColumnType("def"), types::STRING);
}

TEST(RelationTest, mutate_relation) {
  Relation r({types::INT64, types::STRING}, {"abc", "def"});
  r.AddColumn(types::BOOLEAN, "abcd");
  EXPECT_EQ("[abc:int64, def:string, abcd:bool]", r.DebugString());
  EXPECT_EQ(ColTypeArray({types::INT64, types::STRING, types::BOOLEAN}), r.col_types());
  EXPECT_EQ(types::BOOLEAN, r.GetColumnType(2));
  EXPECT_EQ("abcd", r.GetColumnName(2));
  EXPECT_TRUE(r.HasColumn(0));
  EXPECT_TRUE(r.HasColumn(2));
  EXPECT_FALSE(r.HasColumn(3));
  EXPECT_TRUE(r.HasColumn("abc"));
  EXPECT_TRUE(r.HasColumn("def"));
  EXPECT_TRUE(r.HasColumn("abcd"));
  EXPECT_FALSE(r.HasColumn("abcde"));
  EXPECT_EQ(r.GetColumnType("abc"), types::INT64);
  EXPECT_EQ(r.GetColumnType("def"), types::STRING);
  EXPECT_EQ(r.GetColumnType("abcd"), types::BOOLEAN);
}

TEST(RelationDeathTest, out_of_bounds_col_type) {
  Relation r({types::INT64, types::STRING}, {"abc", "def"});
  EXPECT_DEATH(r.GetColumnType(2), ".*does not exist.*");
}

TEST(RelationDeathTest, out_of_bounds_col_name) {
  Relation r({types::INT64, types::STRING}, {"abc", "def"});
  EXPECT_DEATH(r.GetColumnName(2), ".*does not exist.*");
}

TEST(RelationDeathTest, bad_init) {
  EXPECT_DEATH(Relation({types::INT64, types::STRING}, {"abc"}), ".*mismatched.*");
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
