#include <google/protobuf/text_format.h>
#include <memory>
#include <vector>

#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {
class PatternMatchTest : public OperatorTests {};

TEST_F(PatternMatchTest, equals_test) {
  auto c1 = graph->MakeNode<IntIR>(ast).ValueOrDie();
  EXPECT_OK(c1->Init(10));
  auto c2 = graph->MakeNode<IntIR>(ast).ValueOrDie();

  auto agg_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(
      agg_func->Init({FuncIR::Opcode::eq, "==", "equals"}, std::vector<ExpressionIR*>({c1, c2})));

  EXPECT_TRUE(Match(agg_func, Equals(Int(10), Value())));
  EXPECT_TRUE(Match(agg_func, Equals(Value(), Int())));
  EXPECT_FALSE(Match(agg_func, Equals(Value(), Int(9))));
}

TEST_F(PatternMatchTest, compile_time_func_test) {
  auto c1 = graph->MakeNode<IntIR>(ast).ValueOrDie();
  EXPECT_OK(c1->Init(10));
  auto c2 = graph->MakeNode<IntIR>(ast).ValueOrDie();
  EXPECT_OK(c2->Init(9));

  // now()
  auto time_now_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(time_now_func->Init({FuncIR::Opcode::non_op, "", kTimeNowFnStr},
                                std::vector<ExpressionIR*>({})));

  auto not_now_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(not_now_func->Init({FuncIR::Opcode::non_op, "", "not_time_now"},
                               std::vector<ExpressionIR*>({})));

  EXPECT_TRUE(Match(time_now_func, CompileTimeNow()));
  EXPECT_FALSE(Match(not_now_func, CompileTimeNow()));

  // 10 * (10 + 9)
  auto add_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(add_func->Init(FuncIR::op_map["+"], std::vector<ExpressionIR*>({c1, c2})));
  auto mult_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(mult_func->Init(FuncIR::op_map["*"], std::vector<ExpressionIR*>({c1, add_func})));

  auto add_func_no_args = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(
      add_func_no_args->Init({FuncIR::Opcode::add, "+", "add"}, std::vector<ExpressionIR*>({})));

  // hours(10 + 9)
  auto hours_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(hours_func->Init({FuncIR::Opcode::non_op, "", "hours"},
                             std::vector<ExpressionIR*>({add_func})));

  auto no_arg_hours_func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(no_arg_hours_func->Init({FuncIR::Opcode::non_op, "", "hours"},
                                    std::vector<ExpressionIR*>({})));

  EXPECT_TRUE(Match(hours_func, CompileTimeUnitTime()));
  EXPECT_FALSE(Match(no_arg_hours_func, CompileTimeUnitTime()));

  EXPECT_TRUE(Match(mult_func, CompileTimeIntegerArithmetic()));
  EXPECT_FALSE(Match(add_func_no_args, CompileTimeIntegerArithmetic()));

  EXPECT_TRUE(Match(time_now_func, CompileTimeFunc()));
  EXPECT_TRUE(Match(hours_func, CompileTimeFunc()));
  EXPECT_TRUE(Match(mult_func, CompileTimeFunc()));

  EXPECT_FALSE(Match(not_now_func, CompileTimeFunc()));
  EXPECT_FALSE(Match(no_arg_hours_func, CompileTimeFunc()));
  EXPECT_FALSE(Match(add_func_no_args, CompileTimeFunc()));
}

// This bin op test makes sure that non_op doesn't throw errors
// while pattern matching
TEST_F(PatternMatchTest, arbitrary_bin_op_test) {
  auto c1 = graph->MakeNode<IntIR>(ast).ValueOrDie();
  EXPECT_OK(c1->Init(10));
  auto c2 = graph->MakeNode<IntIR>(ast).ValueOrDie();

  auto func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::non_op, "", "op"}, std::vector<ExpressionIR*>({c1, c2})));

  EXPECT_FALSE(Match(func, Equals(Int(10), Value())));
  EXPECT_TRUE(Match(func, BinOp(Value(), Value())));
  EXPECT_TRUE(Match(func, BinOp()));
}

TEST_F(PatternMatchTest, expression_data_type_resolution) {
  auto int1 = graph->MakeNode<IntIR>(ast).ValueOrDie();
  EXPECT_OK(int1->Init(10));
  auto col1 = graph->MakeNode<ColumnIR>(ast).ValueOrDie();
  EXPECT_OK(col1->Init("col1", /* parent_op_idx */ 0));
  auto func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
  EXPECT_OK(
      func->Init({FuncIR::Opcode::non_op, "", "op"}, std::vector<ExpressionIR*>({int1, col1})));

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
  auto mem_src = graph->MakeNode<MemorySourceIR>(ast).ValueOrDie();
  auto blocking_agg = graph->MakeNode<BlockingAggIR>(ast).ValueOrDie();
  EXPECT_OK(blocking_agg->AddParent(mem_src));
  auto map = graph->MakeNode<MapIR>(ast).ValueOrDie();
  EXPECT_OK(map->AddParent(mem_src));
  auto filter = graph->MakeNode<FilterIR>(ast).ValueOrDie();
  EXPECT_OK(filter->AddParent(mem_src));

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

TEST_F(PatternMatchTest, relation_status_union_test) {
  MemorySourceIR* mem_src1 = MakeMemSource();
  MemorySourceIR* mem_src2 = MakeMemSource();

  UnionIR* union_op = MakeUnion({mem_src1, mem_src2});

  EXPECT_FALSE(Match(union_op, UnresolvedReadyUnion()));

  EXPECT_OK(mem_src1->SetRelation(MakeRelation()));

  // Check to make sure that one parent doesn't set it off.
  EXPECT_FALSE(Match(union_op, UnresolvedReadyUnion()));

  EXPECT_OK(mem_src2->SetRelation(MakeRelation()));
  // Check to make sure that setting both parents does set it off.
  EXPECT_TRUE(Match(union_op, UnresolvedReadyUnion()));
}

TEST_F(PatternMatchTest, OpWithParentMatch) {
  auto mem_src1 = MakeMemSource();
  auto group = MakeGroupBy(mem_src1, {MakeColumn("c", 0)});
  auto agg = MakeBlockingAgg(group, {}, {{"a", MakeMeanFunc(MakeColumn("b", 0))}});

  EXPECT_TRUE(Match(agg, OperatorWithParent(BlockingAgg(), GroupBy())));
  EXPECT_TRUE(Match(group, OperatorWithParent(GroupBy(), MemorySource())));
  EXPECT_FALSE(Match(group, OperatorWithParent(BlockingAgg(), GroupBy())));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
