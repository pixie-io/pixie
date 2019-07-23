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

  EXPECT_TRUE(Match(agg_func, Equals(Int(10), Value())));
  EXPECT_TRUE(Match(agg_func, Equals(Value(), Int())));
  EXPECT_FALSE(Match(agg_func, Equals(Value(), Int(9))));
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

  EXPECT_FALSE(Match(func, Equals(Int(10), Value())));
  EXPECT_TRUE(Match(func, BinOp(Value(), Value())));
  EXPECT_TRUE(Match(func, BinOp()));
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
  EXPECT_TRUE(Match(int1, Expression()));
  EXPECT_TRUE(Match(col1, Expression()));
  EXPECT_TRUE(Match(func, Expression()));

  // Make sure unresolved expression works.
  EXPECT_FALSE(Match(int1, UnresolvedExpression()));
  EXPECT_TRUE(Match(col1, UnresolvedExpression()));
  EXPECT_TRUE(Match(func, UnresolvedExpression()));

  // Make sure resolved expression works.
  EXPECT_TRUE(Match(int1, ResolvedExpression()));
  EXPECT_FALSE(Match(col1, ResolvedExpression()));
  EXPECT_FALSE(Match(func, ResolvedExpression()));

  // Specific expressions
  EXPECT_TRUE(Match(col1, UnresolvedColumnType()));
  EXPECT_FALSE(Match(func, UnresolvedColumnType()));
  EXPECT_FALSE(Match(col1, UnresolvedFuncType()));
  EXPECT_TRUE(Match(func, UnresolvedFuncType()));

  // Test out UnresolvedRTFuncMatchAllArgs.
  EXPECT_FALSE(Match(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression())));

  // Resolve column and check whether test works.
  col1->ResolveColumn(0, types::DataType::INT64);
  EXPECT_TRUE(Match(col1, ResolvedExpression()));
  EXPECT_TRUE(Match(col1, ResolvedColumnType()));

  // Should Pass now
  EXPECT_TRUE(Match(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression())));

  // Make sure that resolution works
  func->SetOutputDataType(types::DataType::INT64);
  EXPECT_TRUE(Match(func, ResolvedExpression()));
  EXPECT_TRUE(Match(func, ResolvedFuncType()));
}

TEST_F(PatternMatchTest, relation_status_operator_match) {
  table_store::schema::Relation test_relation;
  test_relation.AddColumn(types::DataType::INT64, "col1");
  test_relation.AddColumn(types::DataType::INT64, "col2");
  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto blocking_agg = graph->MakeNode<BlockingAggIR>().ValueOrDie();
  EXPECT_OK(blocking_agg->SetParent(mem_src));
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  EXPECT_OK(map->SetParent(mem_src));
  auto filter = graph->MakeNode<FilterIR>().ValueOrDie();
  EXPECT_OK(filter->SetParent(mem_src));
  // Unresolved blocking aggregate with unresolved parent.
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyMap()));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp()));
  // Unesolved map with unresolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(map, UnresolvedReadyMap()));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp()));
  // Unresolved Filter with unresolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(filter, UnresolvedReadyMap()));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp()));

  // Resolve parent.
  EXPECT_OK(mem_src->SetRelation(test_relation));
  // Unresolved blocking aggregate with resolved parent.
  EXPECT_TRUE(Match(blocking_agg, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyMap()));
  EXPECT_TRUE(Match(blocking_agg, UnresolvedReadyOp()));
  // Unresolved map with resolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyBlockingAgg()));
  EXPECT_TRUE(Match(map, UnresolvedReadyMap()));
  EXPECT_TRUE(Match(map, UnresolvedReadyOp()));
  // Unresolved Filter with resolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(filter, UnresolvedReadyMap()));
  EXPECT_TRUE(Match(filter, UnresolvedReadyOp()));

  // Resolve children.
  EXPECT_OK(blocking_agg->SetRelation(test_relation));
  EXPECT_OK(map->SetRelation(test_relation));
  EXPECT_OK(filter->SetRelation(test_relation));
  // Resolved blocking aggregate with resolved parent.
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyMap()));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp()));
  // Resolved map with resolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(map, UnresolvedReadyMap()));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp()));
  // Resolved Filter with resolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyBlockingAgg()));
  EXPECT_FALSE(Match(filter, UnresolvedReadyMap()));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp()));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
