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
  auto c1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto c2 = graph->CreateNode<IntIR>(ast, 11).ValueOrDie();

  auto func = MakeEqualsFunc(c1, c2);

  EXPECT_TRUE(Match(func, Equals(Int(10), Value())));
  EXPECT_TRUE(Match(func, Equals(Value(), Int())));
  EXPECT_FALSE(Match(func, Equals(Value(), Int(9))));
}

TEST_F(PatternMatchTest, compile_time_func_test) {
  auto c1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto c2 = graph->CreateNode<IntIR>(ast, 9).ValueOrDie();

  // now()
  auto time_now_func =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", kTimeNowFnStr},
                               std::vector<ExpressionIR*>{})
          .ValueOrDie();

  auto not_now_func =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "not_time_now"},
                               std::vector<ExpressionIR*>{})
          .ValueOrDie();

  EXPECT_TRUE(Match(time_now_func, CompileTimeNow()));
  EXPECT_FALSE(Match(not_now_func, CompileTimeNow()));

  // 10 * (10 + 9)
  auto add_func = MakeAddFunc(c1, c2);
  auto mult_func = MakeMultFunc(c1, add_func);

  auto add_func_no_args = graph
                              ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                                   std::vector<ExpressionIR*>{})
                              .ValueOrDie();

  // hours(10 + 9)
  auto hours_func = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "hours"},
                                             std::vector<ExpressionIR*>{add_func})
                        .ValueOrDie();
  auto no_arg_hours_func =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "hours"},
                               std::vector<ExpressionIR*>{})
          .ValueOrDie();

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

TEST_F(PatternMatchTest, contains_compile_time_func_test) {
  auto c1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto c2 = graph->CreateNode<IntIR>(ast, 9).ValueOrDie();
  auto col1 = graph->CreateNode<ColumnIR>(ast, "z", /* parent_op_idx */ 0).ValueOrDie();

  // 10 + 9 -> true
  auto ct_add = MakeAddFunc(c1, c2);
  EXPECT_TRUE(Match(ct_add, ContainsCompileTimeFunc()));

  // pl.not_compile_time(10 + 9) -> true
  auto rt_func =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "not_compile_time"},
                               std::vector<ExpressionIR*>({ct_add}))
          .ConsumeValueOrDie();
  EXPECT_TRUE(Match(rt_func, ContainsCompileTimeFunc()));

  // 10 + t1['foo'] -> false
  auto rt_add = graph
                    ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("+")->second,
                                         std::vector<ExpressionIR*>({c1, col1}))
                    .ConsumeValueOrDie();
  EXPECT_FALSE(Match(rt_add, ContainsCompileTimeFunc()));

  // pl.not_compile_time(10) + pl.not_compile_time(9) -> false
  auto rt_func_1 =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "not_compile_time"},
                               std::vector<ExpressionIR*>({c1}))
          .ConsumeValueOrDie();
  auto rt_func_2 =
      graph
          ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "not_compile_time"},
                               std::vector<ExpressionIR*>({c1}))
          .ConsumeValueOrDie();
  auto rt_add_2 = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("+")->second,
                                           std::vector<ExpressionIR*>({rt_func_1, rt_func_2}))
                      .ConsumeValueOrDie();
  EXPECT_FALSE(Match(rt_add_2, ContainsCompileTimeFunc()));
}

// This bin op test makes sure that non_op doesn't throw errors
// while pattern matching
TEST_F(PatternMatchTest, arbitrary_bin_op_test) {
  auto c1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto c2 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();

  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "op"},
                                       std::vector<ExpressionIR*>{c1, c2})
                  .ValueOrDie();

  EXPECT_FALSE(Match(func, Equals(Int(10), Value())));
  EXPECT_TRUE(Match(func, BinOp(Value(), Value())));
  EXPECT_TRUE(Match(func, BinOp()));
}

TEST_F(PatternMatchTest, expression_data_type_resolution) {
  auto int1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col1 = graph->CreateNode<ColumnIR>(ast, "col1", /* parent_op_idx */ 0).ValueOrDie();
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "op"},
                                       std::vector<ExpressionIR*>{int1, col1})
                  .ValueOrDie();

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
  col1->ResolveColumnType(types::DataType::INT64);
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
  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  auto blocking_agg =
      graph
          ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{}, ColExpressionVector{})
          .ValueOrDie();
  auto map =
      graph->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{}, /*keep_input_columns*/ false)
          .ValueOrDie();
  ExpressionIR* filter_expr = graph->CreateNode<BoolIR>(ast, false).ValueOrDie();
  auto filter = graph->CreateNode<FilterIR>(ast, mem_src, filter_expr).ValueOrDie();

  // Unresolved blocking aggregate with unresolved parent.
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp()));
  // Unesolved map with unresolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp()));
  // Unresolved Filter with unresolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp()));

  // Resolve parent.
  EXPECT_OK(mem_src->SetRelation(test_relation));
  // Unresolved blocking aggregate with resolved parent.
  EXPECT_TRUE(Match(blocking_agg, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(Map())));
  EXPECT_TRUE(Match(blocking_agg, UnresolvedReadyOp()));
  // Unresolved map with resolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_TRUE(Match(map, UnresolvedReadyOp(Map())));
  EXPECT_TRUE(Match(map, UnresolvedReadyOp()));
  // Unresolved Filter with resolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(Map())));
  EXPECT_TRUE(Match(filter, UnresolvedReadyOp()));

  // Resolve children.
  EXPECT_OK(blocking_agg->SetRelation(test_relation));
  EXPECT_OK(map->SetRelation(test_relation));
  EXPECT_OK(filter->SetRelation(test_relation));
  // Resolved blocking aggregate with resolved parent.
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp()));
  // Resolved map with resolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp()));
  // Resolved Filter with resolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp()));
}

TEST_F(PatternMatchTest, relation_status_union_test) {
  MemorySourceIR* mem_src1 = MakeMemSource();
  MemorySourceIR* mem_src2 = MakeMemSource();

  UnionIR* union_op = MakeUnion({mem_src1, mem_src2});

  EXPECT_FALSE(Match(union_op, UnresolvedReadyOp(Union())));

  EXPECT_OK(mem_src1->SetRelation(MakeRelation()));

  // Check to make sure that one parent doesn't set it off.
  EXPECT_FALSE(Match(union_op, UnresolvedReadyOp(Union())));

  EXPECT_OK(mem_src2->SetRelation(MakeRelation()));
  // Check to make sure that setting both parents does set it off.
  EXPECT_TRUE(Match(union_op, UnresolvedReadyOp(Union())));
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
