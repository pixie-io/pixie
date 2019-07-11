#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_test_utils.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {
class PatternMatchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ast = MakeTestAstPtr();
    graph = std::make_shared<IR>();
  }
  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
};

TEST_F(PatternMatchTest, equals_test) {
  auto c1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(c1->Init(10, ast));
  auto c2 = graph->MakeNode<IntIR>().ValueOrDie();

  auto agg_func = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(agg_func->Init({FuncIR::Opcode::eq, "==", "equals"}, "pl",
                           std::vector<ExpressionIR*>({c1, c2}), false /* compile_time */, ast));

  EXPECT_TRUE(match(agg_func, Equals(Int(10), Value())));
  EXPECT_TRUE(match(agg_func, Equals(Value(), Int())));
  EXPECT_FALSE(match(agg_func, Equals(Value(), Int(9))));
}

// This bin op test makes sure that non_op doesn't throw errors
// while pattern matching
TEST_F(PatternMatchTest, arbitrary_bin_op_test) {
  auto c1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(c1->Init(10, ast));
  auto c2 = graph->MakeNode<IntIR>().ValueOrDie();

  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::non_op, "", "op"}, "pl",
                       std::vector<ExpressionIR*>({c1, c2}), false /* compile_time */, ast));

  EXPECT_FALSE(match(func, Equals(Int(10), Value())));
  EXPECT_TRUE(match(func, BinOp(Value(), Value())));
  EXPECT_TRUE(match(func, BinOp()));
}

TEST_F(PatternMatchTest, expression_data_type_resolution) {
  auto int1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(int1->Init(10, ast));
  auto col1 = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(col1->Init("col1", ast));
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::non_op, "", "op"}, "pl",
                       std::vector<ExpressionIR*>({int1, col1}), false /* compile_time */, ast));

  // Make sure expression works.
  EXPECT_TRUE(match(int1, Expression()));
  EXPECT_TRUE(match(col1, Expression()));
  EXPECT_TRUE(match(func, Expression()));

  // Make sure unresolved expression works.
  EXPECT_FALSE(match(int1, UnresolvedExpression()));
  EXPECT_TRUE(match(col1, UnresolvedExpression()));
  EXPECT_TRUE(match(func, UnresolvedExpression()));

  // Make sure resolved expression works.
  EXPECT_TRUE(match(int1, ResolvedExpression()));
  EXPECT_FALSE(match(col1, ResolvedExpression()));
  EXPECT_FALSE(match(func, ResolvedExpression()));

  // Specific expressions
  EXPECT_TRUE(match(col1, UnresolvedColumn()));
  EXPECT_FALSE(match(func, UnresolvedColumn()));
  EXPECT_FALSE(match(col1, UnresolvedFunc()));
  EXPECT_TRUE(match(func, UnresolvedFunc()));

  // Test out UnresolvedRTFuncMatchAllArgs.
  EXPECT_FALSE(match(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression())));

  // Resolve column and check whether test works.
  col1->ResolveColumn(0, types::DataType::INT64);
  EXPECT_TRUE(match(col1, ResolvedExpression()));
  EXPECT_TRUE(match(col1, ResolvedColumn()));

  // Should Pass now
  EXPECT_TRUE(match(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression())));

  // Make sure that resolution works
  func->SetOutputDataType(types::DataType::INT64);
  EXPECT_TRUE(match(func, ResolvedExpression()));
  EXPECT_TRUE(match(func, ResolvedFunc()));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
