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
using RangeHandlerTest = DataframeTest;

TEST_F(RangeHandlerTest, IntArgs) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  ParsedArgs args;
  args.AddArg("start", MakeInt(12));
  args.AddArg("stop", MakeInt(24));

  auto status = RangeHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);

  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto range_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(range_obj->op(), Range()));
  RangeIR* range = static_cast<RangeIR*>(range_obj->op());
  EXPECT_EQ(range->parents()[0], src);

  ASSERT_TRUE(Match(range->start_repr(), Int()));
  ASSERT_TRUE(Match(range->stop_repr(), Int()));
  EXPECT_EQ(static_cast<IntIR*>(range->start_repr())->val(), 12);
  EXPECT_EQ(static_cast<IntIR*>(range->stop_repr())->val(), 24);
}

TEST_F(RangeHandlerTest, CompileTimeFuncArgs) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto start = MakeSubFunc(MakeFunc("now", {}), MakeFunc("hours", {MakeInt(8)}));
  auto stop = MakeFunc("now", {});

  ParsedArgs args;
  args.AddArg("start", start);
  args.AddArg("stop", stop);

  auto status = RangeHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();

  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto range_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(range_obj->op(), Range()));
  RangeIR* range = static_cast<RangeIR*>(range_obj->op());
  EXPECT_EQ(range->parents()[0], src);

  ASSERT_TRUE(Match(range->start_repr(), Func()));
  ASSERT_TRUE(Match(range->stop_repr(), Func()));
  EXPECT_EQ(static_cast<FuncIR*>(range->start_repr())->carnot_op_name(), "subtract");
  EXPECT_EQ(static_cast<FuncIR*>(range->stop_repr())->carnot_op_name(), "now");
}

TEST_F(RangeHandlerTest, StringArgs) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto start = MakeString("-2m");
  auto stop = MakeString("-1m");

  ParsedArgs args;
  args.AddArg("start", start);
  args.AddArg("stop", stop);

  auto status = RangeHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto range_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(range_obj->op(), Range()));
  RangeIR* range = static_cast<RangeIR*>(range_obj->op());

  ASSERT_TRUE(Match(range->start_repr(), String()));
  ASSERT_TRUE(Match(range->stop_repr(), String()));

  EXPECT_EQ(range->parents()[0], src);
  EXPECT_EQ(static_cast<StringIR*>(range->start_repr())->str(), "-2m");
  EXPECT_EQ(static_cast<StringIR*>(range->stop_repr())->str(), "-1m");
}

TEST_F(RangeHandlerTest, OpArgumentCausesErrorEnd) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto start = MakeString("-2m");
  auto stop = MakeMap(src, {});

  ParsedArgs args;
  args.AddArg("start", start);
  args.AddArg("stop", stop);

  auto status = RangeHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'stop' must be an expression"));
}

TEST_F(RangeHandlerTest, OpArgumentCausesErrorStart) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto stop = MakeString("-2m");
  auto start = MakeMap(src, {});

  ParsedArgs args;
  args.AddArg("start", start);
  args.AddArg("stop", stop);

  auto status = RangeHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'start' must be an expression"));
}

TEST_F(DataframeTest, RangeCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto get_method_status = srcdf->GetMethod("range");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  ArgMap args({{}, {MakeString("-2m"), MakeString("-1m")}});

  std::shared_ptr<QLObject> ql_object = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);

  auto range_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(range_obj->op(), Range()));
  RangeIR* range = static_cast<RangeIR*>(range_obj->op());

  ASSERT_TRUE(Match(range->start_repr(), String()));
  ASSERT_TRUE(Match(range->stop_repr(), String()));

  EXPECT_EQ(range->parents()[0], src);
  EXPECT_EQ(static_cast<StringIR*>(range->start_repr())->str(), "-2m");
  EXPECT_EQ(static_cast<StringIR*>(range->stop_repr())->str(), "-1m");
}

// TODO(philkuz) (PL-1129) figure out default arguments.
TEST_F(DataframeTest, DISABLED_NoEndArgDefault) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto get_method_status = srcdf->GetMethod("range");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  ArgMap args({{{"start", MakeString("-2m")}}, {}});

  std::shared_ptr<QLObject> ql_object = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);

  auto range_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(range_obj->op(), Range()));
  RangeIR* range = static_cast<RangeIR*>(range_obj->op());

  ASSERT_TRUE(Match(range->start_repr(), String()));
  ASSERT_TRUE(Match(range->stop_repr(), Func()));

  EXPECT_EQ(range->parents()[0], src);
  EXPECT_EQ(static_cast<StringIR*>(range->start_repr())->str(), "-2m");
  EXPECT_EQ(static_cast<FuncIR*>(range->stop_repr())->carnot_op_name(), "now");
}

using OldMapTest = DataframeTest;

TEST_F(OldMapTest, CreateOldMap) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto latency_add = MakeAddFunc(MakeColumn("latency_ns", 0), MakeInt(10));
  auto copy_status_column = MakeColumn("http_status", 0);
  auto fn = MakeLambda({{"latency", latency_add}, {"status", copy_status_column}});

  int64_t lambda_id = fn->id();
  ParsedArgs args;
  args.AddArg("fn", fn);

  auto status = OldMapHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto map_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(map_obj->op(), Map()));
  MapIR* map = static_cast<MapIR*>(map_obj->op());

  EXPECT_EQ(map->parents()[0], src);
  EXPECT_EQ(map->col_exprs()[0].node, latency_add);
  EXPECT_EQ(map->col_exprs()[1].node, copy_status_column);

  EXPECT_FALSE(graph->HasNode(lambda_id));
}

TEST_F(OldMapTest, OldMapWithNonLambda) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto latency_add = MakeAddFunc(MakeColumn("latency_ns", 0), MakeInt(10));

  ParsedArgs args;
  args.AddArg("fn", latency_add);

  auto status = OldMapHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'fn' must be a lambda"));
}

TEST_F(OldMapTest, OldMapWithLambdaNonDictBody) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto latency_add = MakeAddFunc(MakeColumn("latency_ns", 0), MakeInt(10));
  auto fn = MakeLambda(latency_add);

  ParsedArgs args;
  args.AddArg("fn", fn);

  auto status = OldMapHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("'fn' argument error, lambda must have a dictionary body"));
}

TEST_F(DataframeTest, OldMapCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto latency_add = MakeAddFunc(MakeColumn("latency_ns", 0), MakeInt(10));
  auto copy_status_column = MakeColumn("http_status", 0);
  auto fn = MakeLambda({{"latency", latency_add}, {"status", copy_status_column}});

  int64_t lambda_id = fn->id();
  ArgMap args({{}, {fn}});

  auto get_method_status = srcdf->GetMethod("map");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto status = func_obj->Call(args, ast);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto map_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(map_obj->op(), Map()));
  MapIR* map = static_cast<MapIR*>(map_obj->op());

  EXPECT_EQ(map->parents()[0], src);
  EXPECT_EQ(map->col_exprs()[0].node, latency_add);
  EXPECT_EQ(map->col_exprs()[1].node, copy_status_column);

  EXPECT_FALSE(graph->HasNode(lambda_id));
}

using OldFilterTest = DataframeTest;

TEST_F(OldFilterTest, CreateOldFilter) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto status_400 = MakeEqualsFunc(MakeColumn("status_code", 0), MakeInt(400));
  auto fn = MakeLambda(status_400);

  int64_t lambda_id = fn->id();
  ParsedArgs args;
  args.AddArg("fn", fn);

  auto status = OldFilterHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto filter_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(filter_obj->op(), Filter()));
  FilterIR* filter = static_cast<FilterIR*>(filter_obj->op());

  EXPECT_EQ(filter->parents()[0], src);

  EXPECT_EQ(filter->filter_expr(), status_400);

  EXPECT_FALSE(graph->HasNode(lambda_id));
}

TEST_F(OldFilterTest, OldFilterWithNonLambda) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto status_400 = MakeEqualsFunc(MakeColumn("status_code", 0), MakeInt(400));

  ParsedArgs args;
  args.AddArg("fn", status_400);

  auto status = OldFilterHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'fn' must be a lambda"));
}

TEST_F(OldFilterTest, OldFilterWithLambdaDictBody) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto status_400 = MakeEqualsFunc(MakeColumn("status_code", 0), MakeInt(400));
  auto fn = MakeLambda({{"status", status_400}});

  ParsedArgs args;
  args.AddArg("fn", fn);

  auto status = OldFilterHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("'fn' argument error, lambda cannot have a dictionary body"));
}

TEST_F(DataframeTest, OldFilterCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto status_400 = MakeEqualsFunc(MakeColumn("status_code", 0), MakeInt(400));
  auto fn = MakeLambda(status_400);

  int64_t lambda_id = fn->id();
  ArgMap args({{}, {fn}});

  auto get_method_status = srcdf->GetMethod("filter");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto status = func_obj->Call(args, ast);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto filter_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(filter_obj->op(), Filter()));
  FilterIR* filter = static_cast<FilterIR*>(filter_obj->op());

  EXPECT_EQ(filter->parents()[0], src);

  EXPECT_EQ(filter->filter_expr(), status_400);

  EXPECT_FALSE(graph->HasNode(lambda_id));
}

using LimitTest = DataframeTest;

TEST_F(LimitTest, CreateLimit) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto limit_value = MakeInt(1234);

  int64_t limit_int_node_id = limit_value->id();
  ParsedArgs args;
  args.AddArg("rows", limit_value);

  auto status = LimitHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto limit_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(limit_obj->op(), Limit()));
  LimitIR* limit = static_cast<LimitIR*>(limit_obj->op());
  EXPECT_EQ(limit->limit_value(), 1234);

  EXPECT_FALSE(graph->HasNode(limit_int_node_id));
}

TEST_F(LimitTest, LimitNonIntArgument) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto limit_value = MakeString("1234");

  ParsedArgs args;
  args.AddArg("rows", limit_value);

  auto status = LimitHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'rows' must be an int"));
}

TEST_F(DataframeTest, LimitCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto limit_value = MakeInt(1234);

  int64_t limit_int_node_id = limit_value->id();
  ArgMap args{{{"rows", limit_value}}, {}};

  auto get_method_status = srcdf->GetMethod("limit");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto status = func_obj->Call(args, ast);
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto limit_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(limit_obj->op(), Limit()));
  LimitIR* limit = static_cast<LimitIR*>(limit_obj->op());
  EXPECT_EQ(limit->limit_value(), 1234);

  EXPECT_FALSE(graph->HasNode(limit_int_node_id));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
