#include <gtest/gtest.h>
#include <memory>

#include "absl/container/flat_hash_map.h"

#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::testing::ElementsAre;

class DataframeTest : public OperatorTests {};

TEST_F(DataframeTest, MergeTest) {
  MemorySourceIR* left = MakeMemSource();
  MemorySourceIR* right = MakeMemSource();
  std::shared_ptr<QLObject> test = std::make_shared<Dataframe>(left);
  auto get_method_status = test->GetMethod("merge");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  ArgMap args({{{"suffixes", MakeList(MakeString("_x"), MakeString("_y"))}},
               {right, MakeString("inner"), MakeList(MakeString("a"), MakeString("b")),
                MakeList(MakeString("b"), MakeString("c"))}});

  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  // Add compartor for type() and Dataframe.
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kDataframe);
  auto df_obj = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the output is a Join operator.
  OperatorIR* op = df_obj->op();
  ASSERT_TRUE(Match(op, Join()));
  JoinIR* join = static_cast<JoinIR*>(op);
  // Verify that the operator does what we expect it to.
  EXPECT_THAT(join->parents(), ElementsAre(left, right));
  EXPECT_EQ(join->join_type(), JoinIR::JoinType::kInner);

  EXPECT_TRUE(Match(join->left_on_columns()[0], ColumnNode("a", 0)));
  EXPECT_TRUE(Match(join->left_on_columns()[1], ColumnNode("b", 0)));

  EXPECT_TRUE(Match(join->right_on_columns()[0], ColumnNode("b", 1)));
  EXPECT_TRUE(Match(join->right_on_columns()[1], ColumnNode("c", 1)));

  EXPECT_THAT(join->suffix_strs(), ElementsAre("_x", "_y"));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
