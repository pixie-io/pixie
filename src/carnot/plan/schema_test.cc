#include <gtest/gtest.h>

#include "src/carnot/plan/schema.h"

namespace pl {
namespace carnot {
namespace plan {

class SchemaTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    rel1_.AddColumn(planpb::INT64, "abc");
    rel2_.AddColumn(planpb::STRING, "def");
  }

  Relation rel1_;
  Relation rel2_;
};

TEST_F(SchemaTest, no_relations) {
  Schema s;
  EXPECT_FALSE(s.HasRelation(0));
  EXPECT_FALSE(s.HasRelation(1));
  EXPECT_EQ("Relation: <empty>", s.DebugString());
}

TEST_F(SchemaTest, new_relations) {
  Schema s;
  s.AddRelation(123, rel1_);
  EXPECT_FALSE(s.HasRelation(0));
  EXPECT_TRUE(s.HasRelation(123));
  EXPECT_EQ(std::vector<int64_t>({123}), s.GetIDs());
  EXPECT_EQ("Relation:\n  {123} : [abc:int64]\n", s.DebugString());

  s.AddRelation(256, rel2_);
  EXPECT_FALSE(s.HasRelation(0));
  EXPECT_TRUE(s.HasRelation(123));
  EXPECT_TRUE(s.HasRelation(256));
  EXPECT_EQ(std::vector<int64_t>({123, 256}), s.GetIDs());
  EXPECT_EQ("Relation:\n  {123} : [abc:int64]\n  {256} : [def:string]\n", s.DebugString());
}

TEST_F(SchemaTest, overwrite_relation) {
  Schema s;
  s.AddRelation(123, rel1_);
  s.AddRelation(123, rel2_);
  EXPECT_FALSE(s.HasRelation(0));
  EXPECT_TRUE(s.HasRelation(123));
  EXPECT_EQ(std::vector<int64_t>({123}), s.GetIDs());
  EXPECT_EQ("Relation:\n  {123} : [def:string]\n", s.DebugString());
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
