#include <gtest/gtest.h>
#include <memory>

#include "absl/container/flat_hash_map.h"

#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/metadata_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using DataframeTest = QLObjectTest;

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

  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast, ast_visitor.get()).ConsumeValueOrDie();
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

using JoinHandlerTest = DataframeTest;
TEST_F(JoinHandlerTest, MergeTest) {
  MemorySourceIR* left = MakeMemSource();
  MemorySourceIR* right = MakeMemSource();
  std::shared_ptr<Dataframe> test = std::make_shared<Dataframe>(left);

  ParsedArgs args;
  args.AddArg("suffixes", MakeList(MakeString("_x"), MakeString("_y")));
  args.AddArg("right", right);
  args.AddArg("how", MakeString("inner"));
  args.AddArg("left_on", MakeList(MakeString("a"), MakeString("b")));
  args.AddArg("right_on", MakeList(MakeString("b"), MakeString("c")));

  auto status = JoinHandler::Eval(test.get(), ast, args);
  ASSERT_OK(status);
  std::shared_ptr<QLObject> obj = status.ConsumeValueOrDie();
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

using DropHandlerTest = DataframeTest;
TEST_F(DropHandlerTest, DropTest) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);
  ParsedArgs args;
  args.AddArg("columns", MakeList(MakeString("foo"), MakeString("bar")));
  auto status = DropHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);

  std::shared_ptr<QLObject> obj = status.ConsumeValueOrDie();
  // Add compartor for type() and Dataframe.
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kDataframe);
  auto df_obj = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the output is a Join operator.
  OperatorIR* op = df_obj->op();
  ASSERT_TRUE(Match(op, Drop()));
  DropIR* drop = static_cast<DropIR*>(op);
  // Verify that the operator does what we expect it to.
  EXPECT_EQ(drop->col_names(), std::vector<std::string>({"foo", "bar"}));
}

TEST_F(DropHandlerTest, DropTestNonString) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);
  ParsedArgs args;
  args.AddArg("columns", MakeList(MakeInt(1)));
  auto status = DropHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("The elements of the list must be Strings, not"));
}

// TODO(philkuz) (PL-1128) re-enable when we switch from old agg to new agg.
TEST_F(DataframeTest, DISABLED_AggTest) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<QLObject> srcdf = std::make_shared<Dataframe>(src);
  auto get_method_status = srcdf->GetMethod("agg");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  ArgMap args({{{"out_col1", MakeTuple(MakeString("col1"), MakeMeanFunc())},
                {"out_col2", MakeTuple(MakeString("col2"), MakeMeanFunc())}},
               {}});

  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast, ast_visitor.get()).ConsumeValueOrDie();
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

TEST_F(DataframeTest, DISABLED_AggFailsWithPosArgs) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<QLObject> srcdf = std::make_shared<Dataframe>(src);
  auto get_method_status = srcdf->GetMethod("agg");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  // Only positional arguments
  ArgMap args({{}, {MakeTuple(MakeString("col1"), MakeMeanFunc())}});

  auto call_status = func_obj->Call(args, ast, ast_visitor.get());
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
  EXPECT_THAT(status.status(), HasCompilerError("Unexpected aggregate function"));
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

using LimitTest = DataframeTest;

TEST_F(LimitTest, CreateLimit) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto limit_value = MakeInt(1234);

  int64_t limit_int_node_id = limit_value->id();
  ParsedArgs args;
  args.AddArg("n", limit_value);

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
  args.AddArg("n", limit_value);

  auto status = LimitHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'n' must be an int"));
}

TEST_F(DataframeTest, LimitCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  auto limit_value = MakeInt(1234);

  int64_t limit_int_node_id = limit_value->id();
  ArgMap args{{{"n", limit_value}}, {}};

  auto get_method_status = srcdf->GetMethod("head");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto status = func_obj->Call(args, ast, ast_visitor.get());
  ASSERT_OK(status);
  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto limit_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(limit_obj->op(), Limit()));
  LimitIR* limit = static_cast<LimitIR*>(limit_obj->op());
  EXPECT_EQ(limit->limit_value(), 1234);

  EXPECT_FALSE(graph->HasNode(limit_int_node_id));
}

using OldResultTest = DataframeTest;

TEST_F(OldResultTest, SimpleResultCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  ParsedArgs args;
  args.AddArg("name", MakeString("foo"));

  auto status = OldResultHandler::Eval(srcdf.get(), ast, args);
  ASSERT_OK(status);

  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kNone);
  auto none_obj = std::static_pointer_cast<NoneObject>(ql_object);

  ASSERT_TRUE(none_obj->HasNode());
  ASSERT_TRUE(Match(none_obj->node(), MemorySink()));
  MemorySinkIR* sink = static_cast<MemorySinkIR*>(none_obj->node());
  EXPECT_THAT(sink->parents(), ElementsAre(src));
  EXPECT_EQ(sink->name(), "foo");
}

TEST_F(OldResultTest, ResultCallWrongNameType) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  ParsedArgs args;
  args.AddArg("name", MakeInt(123));

  auto status = OldResultHandler::Eval(srcdf.get(), ast, args);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("'name' must be a str"));
}

TEST_F(DataframeTest, ResultCall) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<Dataframe> srcdf = std::make_shared<Dataframe>(src);

  ArgMap args{{}, {MakeString("foo")}};

  auto get_method_status = srcdf->GetMethod(Dataframe::kSinkOpId);
  ASSERT_OK(get_method_status);
  auto func_obj = get_method_status.ConsumeValueOrDie();
  auto status = func_obj->Call(args, ast, ast_visitor.get());
  ASSERT_OK(status);

  QLObjectPtr ql_object = status.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kNone);
  auto none_obj = std::static_pointer_cast<NoneObject>(ql_object);

  ASSERT_TRUE(none_obj->HasNode());
  ASSERT_TRUE(Match(none_obj->node(), MemorySink()));
  MemorySinkIR* sink = static_cast<MemorySinkIR*>(none_obj->node());
  EXPECT_THAT(sink->parents(), ElementsAre(src));
  EXPECT_EQ(sink->name(), "foo");
}

class SubscriptTest : public DataframeTest {
 protected:
  void SetUp() override {
    DataframeTest::SetUp();
    src = MakeMemSource();
    srcdf = std::make_shared<Dataframe>(src);
  }

  std::shared_ptr<Dataframe> srcdf;
  MemorySourceIR* src;
};

TEST_F(SubscriptTest, FilterCanTakeExpr) {
  ParsedArgs parsed_args;
  auto eq_func = MakeEqualsFunc(MakeColumn("service", 0), MakeString("blah"));
  parsed_args.AddArg("key", eq_func);

  auto qlo_or_s = SubscriptHandler::Eval(srcdf.get(), ast, parsed_args);
  ASSERT_OK(qlo_or_s);
  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto filter_obj = std::static_pointer_cast<Dataframe>(ql_object);

  FilterIR* filt_ir = static_cast<FilterIR*>(filter_obj->op());
  EXPECT_EQ(filt_ir->filter_expr(), eq_func);
}

TEST_F(SubscriptTest, DataframeHasFilterAsGetItem) {
  auto eq_func = MakeEqualsFunc(MakeColumn("service", 0), MakeString("blah"));
  ArgMap args{{}, {eq_func}};

  auto get_method_status = srcdf->GetSubscriptMethod();
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto qlo_or_s = func_obj->Call(args, ast, ast_visitor.get());

  ASSERT_OK(qlo_or_s);
  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto filter_obj = std::static_pointer_cast<Dataframe>(ql_object);

  FilterIR* filt_ir = static_cast<FilterIR*>(filter_obj->op());
  EXPECT_EQ(filt_ir->filter_expr(), eq_func);
}

TEST_F(SubscriptTest, KeepTest) {
  ParsedArgs parsed_args;
  auto keep_list = MakeList(MakeString("foo"), MakeString("bar"));
  parsed_args.AddArg("key", keep_list);

  auto qlo_or_s = SubscriptHandler::Eval(srcdf.get(), ast, parsed_args);
  ASSERT_OK(qlo_or_s);
  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto map_obj = std::static_pointer_cast<Dataframe>(ql_object);

  MapIR* map_ir = static_cast<MapIR*>(map_obj->op());
  EXPECT_EQ(map_ir->col_exprs().size(), 2);
  EXPECT_EQ(map_ir->col_exprs()[0].name, "foo");
  EXPECT_EQ(map_ir->col_exprs()[1].name, "bar");
}

TEST_F(SubscriptTest, DataframeHasKeepAsGetItem) {
  auto keep_list = MakeList(MakeString("foo"), MakeString("bar"));
  ArgMap args{{}, {keep_list}};

  auto get_method_status = srcdf->GetSubscriptMethod();
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto qlo_or_s = func_obj->Call(args, ast, ast_visitor.get());

  ASSERT_OK(qlo_or_s);
  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto map_obj = std::static_pointer_cast<Dataframe>(ql_object);

  MapIR* map_ir = static_cast<MapIR*>(map_obj->op());
  EXPECT_EQ(map_ir->col_exprs().size(), 2);
  EXPECT_EQ(map_ir->col_exprs()[0].name, "foo");
  EXPECT_EQ(map_ir->col_exprs()[1].name, "bar");
}

TEST_F(SubscriptTest, SubscriptCanHandleErrorInput) {
  ParsedArgs parsed_args;
  auto node = MakeMemSource();
  parsed_args.AddArg("key", node);

  auto qlo_or_s = SubscriptHandler::Eval(srcdf.get(), ast, parsed_args);
  ASSERT_NOT_OK(qlo_or_s);

  EXPECT_THAT(qlo_or_s.status(),
              HasCompilerError("subscript argument must have an expression. '.*' not allowed"));
}

class GroupByTest : public DataframeTest {
 protected:
  void SetUp() override {
    DataframeTest::SetUp();
    src = MakeMemSource();
    srcdf = std::make_shared<Dataframe>(src);
  }

  std::shared_ptr<Dataframe> srcdf;
  MemorySourceIR* src;
};

TEST_F(GroupByTest, GroupByList) {
  auto list = MakeList(MakeString("col1"), MakeString("col2"));
  ParsedArgs parsed_args;
  parsed_args.AddArg("by", list);
  auto qlo_or_s = GroupByHandler::Eval(srcdf.get(), ast, parsed_args);

  ASSERT_OK(qlo_or_s);

  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto group_by_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(group_by_obj->op(), GroupBy()));
  GroupByIR* group_by = static_cast<GroupByIR*>(group_by_obj->op());
  EXPECT_EQ(group_by->groups().size(), 2);
  EXPECT_TRUE(Match(group_by->groups()[0], ColumnNode("col1", 0)));
  EXPECT_TRUE(Match(group_by->groups()[1], ColumnNode("col2", 0)));
}

TEST_F(GroupByTest, GroupByString) {
  auto str = MakeString("col1");
  ParsedArgs parsed_args;
  parsed_args.AddArg("by", str);
  auto qlo_or_s = GroupByHandler::Eval(srcdf.get(), ast, parsed_args);

  ASSERT_OK(qlo_or_s);

  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto group_by_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(group_by_obj->op(), GroupBy()));
  GroupByIR* group_by = static_cast<GroupByIR*>(group_by_obj->op());
  EXPECT_EQ(group_by->groups().size(), 1);
  EXPECT_TRUE(Match(group_by->groups()[0], ColumnNode("col1", 0)));
}

TEST_F(GroupByTest, GroupByMixedListElementTypesCausesError) {
  auto list = MakeList(MakeString("col1"), MakeInt(2));
  ParsedArgs parsed_args;
  parsed_args.AddArg("by", list);
  auto qlo_or_s = GroupByHandler::Eval(srcdf.get(), ast, parsed_args);

  ASSERT_NOT_OK(qlo_or_s);
  EXPECT_THAT(qlo_or_s.status(), HasCompilerError("'by' expected string or list of strings"));
}

TEST_F(GroupByTest, GroupByInDataframe) {
  auto str = MakeString("col1");
  ArgMap args{{}, {str}};

  auto get_method_status = srcdf->GetMethod("groupby");
  ASSERT_OK(get_method_status);
  FuncObject* func_obj = static_cast<FuncObject*>(get_method_status.ConsumeValueOrDie().get());
  auto qlo_or_s = func_obj->Call(args, ast, ast_visitor.get());
  ASSERT_OK(qlo_or_s);

  QLObjectPtr ql_object = qlo_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(ql_object->type_descriptor().type() == QLObjectType::kDataframe);
  auto group_by_obj = std::static_pointer_cast<Dataframe>(ql_object);

  ASSERT_TRUE(Match(group_by_obj->op(), GroupBy()));
  GroupByIR* group_by = static_cast<GroupByIR*>(group_by_obj->op());
  EXPECT_EQ(group_by->groups().size(), 1);
  EXPECT_TRUE(Match(group_by->groups()[0], ColumnNode("col1", 0)));
}

TEST_F(DataframeTest, AttributeMetadataSubscriptTest) {
  MemorySourceIR* src = MakeMemSource();
  std::shared_ptr<QLObject> test = std::make_shared<Dataframe>(src);
  EXPECT_TRUE(test->HasAttribute(Dataframe::kMetadataAttrName));
  auto attr_or_s = test->GetAttribute(ast, Dataframe::kMetadataAttrName);

  ASSERT_OK(attr_or_s);

  QLObjectPtr ptr = attr_or_s.ConsumeValueOrDie();
  EXPECT_TRUE(ptr->type_descriptor().type() == QLObjectType::kMetadata);
  auto metadata = static_cast<MetadataObject*>(ptr.get());
  ASSERT_TRUE(metadata->HasSubscriptMethod());
  std::shared_ptr<FuncObject> func = metadata->GetSubscriptMethod().ConsumeValueOrDie();

  auto func_result =
      func->Call(ArgMap{{}, {MakeString("service")}}, ast, ast_visitor.get()).ConsumeValueOrDie();
  ASSERT_TRUE(func_result->type_descriptor().type() == QLObjectType::kExpr);
  auto metadata_expr = static_cast<ExprObject*>(func_result.get());
  ASSERT_TRUE(metadata_expr->HasNode());
  ASSERT_TRUE(Match(metadata_expr->node(), Metadata()));
  auto metadata_node = static_cast<MetadataIR*>(metadata_expr->node());
  EXPECT_EQ(metadata_node->name(), "service");
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
