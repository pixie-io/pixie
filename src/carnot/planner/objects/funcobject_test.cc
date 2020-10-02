#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
using PyFuncTest = QLObjectTest;

class TestQLObject : public QLObject {
 public:
  static constexpr TypeDescriptor TestQLObjectType = {
      /* name */ "TestQLObject",
      /* type */ QLObjectType::kMisc,
  };
  TestQLObject(int64_t value, ASTVisitor* visitor)
      : QLObject(TestQLObjectType, visitor), value_(value) {}

  // Value used for testing that we can initialize QLObjects within a FuncObject
  int64_t value() { return value_; }
  void AddKwargValue(const std::string& arg_name, const std::string& value) {
    kwarg_names_.push_back(arg_name);
    kwarg_values_.push_back(value);
  }
  const std::vector<std::string>& kwarg_names() const { return kwarg_names_; }
  const std::vector<std::string>& kwarg_values() const { return kwarg_values_; }

 private:
  int64_t value_;
  std::vector<std::string> kwarg_names_;
  std::vector<std::string> kwarg_values_;
};

StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr& ast, const ParsedArgs& args,
                                 ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(IntIR * node, GetArgAs<IntIR>(ast, args, "simple"));
  auto out_obj = std::make_shared<TestQLObject>(node->val(), visitor);
  for (const auto& [arg, val] : args.kwargs()) {
    auto value = val->node();
    if (!Match(value, String())) {
      return value->CreateIRNodeError("Expected string $0", value->DebugString());
    }
    out_obj->AddKwargValue(arg, static_cast<StringIR*>(value)->str());
  }

  return StatusOr<QLObjectPtr>(out_obj);
}

StatusOr<QLObjectPtr> SimpleFuncForVarArgs(const pypa::AstPtr& ast, const ParsedArgs& args,
                                           ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(IntIR * node, GetArgAs<IntIR>(ast, args, "simple"));
  auto out_obj = std::make_shared<TestQLObject>(static_cast<IntIR*>(node)->val(), visitor);
  for (const auto& [idx, val] : Enumerate(args.variable_args())) {
    auto value = val->node();
    if (!Match(value, String())) {
      return value->CreateIRNodeError("Expected string $0", value->DebugString());
    }
    out_obj->AddKwargValue(absl::Substitute("arg$0", idx), static_cast<StringIR*>(value)->str());
  }

  for (const auto& [arg, val] : args.kwargs()) {
    auto value = val->node();
    if (!Match(value, String())) {
      return value->CreateIRNodeError("Expected string $0", value->DebugString());
    }
    out_obj->AddKwargValue(arg, static_cast<StringIR*>(value)->str());
  }

  return StatusOr<QLObjectPtr>(out_obj);
}

TEST_F(PyFuncTest, PosArgsExecute) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args = MakeArgMap({}, {MakeInt(123)});
  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kMisc);
  auto test_obj = static_cast<TestQLObject*>(obj.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre());
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre());
}

TEST_F(PyFuncTest, MissingArgument) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args;
  auto status = func_obj->Call(args, ast);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("func.* missing 1 required positional arguments 'simple'"));
}

TEST_F(PyFuncTest, ExtraPosArg) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args = MakeArgMap({}, {MakeInt(1), MakeInt(2)});
  auto status = func_obj->Call(args, ast);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("func.* takes 1 arguments but 2 were given."));

  // Should do the same with kwarg support enabled.
  std::shared_ptr<FuncObject> func_obj2 =
      FuncObject::Create("func", {"simple"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ true,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  auto status2 = func_obj2->Call(args, ast);
  ASSERT_NOT_OK(status2);
  EXPECT_THAT(status2.status(), HasCompilerError("func.* takes 1 arguments but 2 were given."));
}

TEST_F(PyFuncTest, ExtraKwargNoKwargsSupport) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args = MakeArgMap({{"blah", MakeString("blah")}}, {MakeInt(1)});
  auto status = func_obj->Call(args, ast);
  ASSERT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("func.* got an unexpected keyword argument 'blah'"));
}

TEST_F(PyFuncTest, ExtraKwargWithKwargsSupport) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ true,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args = MakeArgMap({{"blah1", MakeString("blah2")}}, {MakeInt(123)});
  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kMisc);
  auto test_obj = static_cast<TestQLObject*>(obj.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre("blah1"));
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre("blah2"));
}

TEST_F(PyFuncTest, DefaultArgsExecute) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args;
  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kMisc);
  auto test_obj = static_cast<TestQLObject*>(obj.get());
  EXPECT_EQ(test_obj->value(), 1234);
}

// This test is the unit to make sure we can get all of the defaults of any method for any object we
// choose to test.
TEST_F(PyFuncTest, TestDefaultArgsCanBeAccessed) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ASSERT_TRUE(func_obj->defaults().contains("simple"));
  auto default_str_repr = func_obj->defaults().find("simple")->second;
  auto expr_or_s =
      ast_visitor->ParseAndProcessSingleExpression(default_str_repr, /*import_px*/ true);
  ASSERT_OK(expr_or_s);
  auto expr = expr_or_s.ConsumeValueOrDie();
  EXPECT_TRUE(expr->HasNode());
  EXPECT_MATCH(expr->node(), Int(1234));
}

// This test makes sure we use variable args.
TEST_F(PyFuncTest, TestVariableArgs) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}}, /* has_variable_args_len */ true,
                         /* has_kwargs */ false,
                         std::bind(&SimpleFuncForVarArgs, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args = MakeArgMap({}, {MakeInt(123), MakeString("str123"), MakeString("str321")});
  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kMisc);
  auto test_obj = static_cast<TestQLObject*>(obj.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre("arg0", "arg1"));
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre("str123", "str321"));
}

// This test makes sure variable args and variable kwargs both work when used together.
TEST_F(PyFuncTest, VariableArgsAndKwargs) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}}, /* has_variable_args_len */ true,
                         /* has_kwargs */ true,
                         std::bind(&SimpleFuncForVarArgs, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  ArgMap args = MakeArgMap({{"kwarg1", MakeString("str222")}, {"kwarg2", MakeString("str333")}},
                           {MakeInt(123), MakeString("str123"), MakeString("str321")});
  std::shared_ptr<QLObject> obj = func_obj->Call(args, ast).ConsumeValueOrDie();
  ASSERT_TRUE(obj->type_descriptor().type() == QLObjectType::kMisc);
  auto test_obj = static_cast<TestQLObject*>(obj.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre("arg0", "arg1", "kwarg1", "kwarg2"));
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre("str123", "str321", "str222", "str333"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
