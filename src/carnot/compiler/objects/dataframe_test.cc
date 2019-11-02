#include <gtest/gtest.h>
#include <memory>

#include "absl/container/flat_hash_map.h"

#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

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

TEST_F(DataframeTest, AggTest) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<QLObject> srcdf = std::make_shared<Dataframe>(src);
  auto get_method_status = srcdf->GetMethod("agg");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  ArgMap args({{{"out_col1", MakeTuple(MakeString("col1"), MakeMeanFunc())},
                {"out_col2", MakeTuple(MakeString("col2"), MakeMeanFunc())}},
               {}});

  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  // Add compartor for type() and Dataframe.
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kDataframe);
  auto df_obj = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the output is a Join operator.
  OperatorIR* op = df_obj->op();
  ASSERT_TRUE(Match(op, BlockingAgg()));
  BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
  // Verify that the operator does what we expect it to.
  EXPECT_THAT(agg->parents(), ElementsAre(src));
  ASSERT_EQ(agg->aggregate_expressions().size(), 2);
  std::vector<std::string> outcol_names{agg->aggregate_expressions()[0].name,
                                        agg->aggregate_expressions()[1].name};
  ASSERT_THAT(outcol_names, UnorderedElementsAre("out_col1", "out_col2"));

  std::vector<std::string> col_names;
  for (const auto& [index, expr] : Enumerate(agg->aggregate_expressions())) {
    ASSERT_TRUE(Match(expr.node, Func())) << index;
    FuncIR* fn = static_cast<FuncIR*>(expr.node);
    EXPECT_EQ(fn->func_name(), "pl.mean") << index;
    ASSERT_EQ(fn->args().size(), 1) << index;
    ASSERT_TRUE(Match(fn->args()[0], ColumnNode())) << index;
    ColumnIR* col = static_cast<ColumnIR*>(fn->args()[0]);
    col_names.push_back(col->col_name());
  }
  ASSERT_THAT(col_names, UnorderedElementsAre("col1", "col2"));
}

TEST_F(DataframeTest, AggFailsWithPosArgs) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<QLObject> srcdf = std::make_shared<Dataframe>(src);
  auto get_method_status = srcdf->GetMethod("agg");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  // Only positional arguments
  ArgMap args({{}, {MakeTuple(MakeString("col1"), MakeMeanFunc())}});

  auto call_status = func_obj->Call(args, ast);
  ASSERT_NOT_OK(call_status);
  EXPECT_THAT(call_status.status(), HasCompilerError("agg.* takes 0 arguments but 1 .* given"));
}

using AggHandlerTest = DataframeTest;

TEST_F(AggHandlerTest, NonTupleKwarg) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);
  ParsedArgs args;
  args.AddKwarg("outcol1", MakeString("fail"));
  auto status = AggHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("Expected 'agg' kwarg argument to be a tuple, not"));
}

TEST_F(AggHandlerTest, NonStrFirstTupleArg) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);
  ParsedArgs args;
  args.AddKwarg("outcol1", MakeTuple(MakeInt(1), MakeMeanFunc()));
  auto status = AggHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("Expected 'str' for first tuple argument"));
}

TEST_F(AggHandlerTest, NonFuncSecondTupleArg) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);
  ParsedArgs args;
  args.AddKwarg("outcol1", MakeTuple(MakeString("ll"), MakeString("dd")));
  auto status = AggHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("Expected 'func' for second tuple argument. Received"));
}

TEST_F(AggHandlerTest, NonZeroArgFuncKwarg) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);
  ParsedArgs args;
  args.AddKwarg("outcol1", MakeTuple(MakeString("ll"), MakeMeanFunc(MakeColumn("str", 0))));
  auto status = AggHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("Expected function to not have specified arguments"));
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
