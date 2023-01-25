/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
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
  PX_ASSIGN_OR_RETURN(IntIR * node, GetArgAs<IntIR>(ast, args, "simple"));
  auto out_obj = std::make_shared<TestQLObject>(node->val(), visitor);
  for (const auto& [arg, val] : args.kwargs()) {
    PX_ASSIGN_OR_RETURN(auto str, GetAsString(val));
    out_obj->AddKwargValue(arg, str);
  }

  return StatusOr<QLObjectPtr>(out_obj);
}

StatusOr<QLObjectPtr> SimpleFuncForVarArgs(const pypa::AstPtr& ast, const ParsedArgs& args,
                                           ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(IntIR * node, GetArgAs<IntIR>(ast, args, "simple"));
  auto out_obj = std::make_shared<TestQLObject>(static_cast<IntIR*>(node)->val(), visitor);
  for (const auto& [idx, val] : Enumerate(args.variable_args())) {
    PX_ASSIGN_OR_RETURN(auto str, GetAsString(val));
    out_obj->AddKwargValue(absl::Substitute("arg$0", idx), str);
  }

  for (const auto& [arg, val] : args.kwargs()) {
    PX_ASSIGN_OR_RETURN(auto str, GetAsString(val));
    out_obj->AddKwargValue(arg, str);
  }

  return StatusOr<QLObjectPtr>(out_obj);
}

class PyFuncTest : public QLObjectTest {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    var_table->Add("simplefunc",
                   FuncObject::Create("func", {"simple"}, {},
                                      /* has_variable_len_args */ false,
                                      /* has_variable_len_kwargs */ false,
                                      std::bind(&SimpleFunc, std::placeholders::_1,
                                                std::placeholders::_2, std::placeholders::_3),
                                      ast_visitor.get())
                       .ConsumeValueOrDie());
  }
};

TEST_F(PyFuncTest, PosArgsExecute) {
  var_table->Add("simplefunc",
                 FuncObject::Create("func", {"simple"}, {},
                                    /* has_variable_len_args */ false,
                                    /* has_variable_len_kwargs */ false,
                                    std::bind(&SimpleFunc, std::placeholders::_1,
                                              std::placeholders::_2, std::placeholders::_3),
                                    ast_visitor.get())
                     .ConsumeValueOrDie());
  ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("simplefunc(123)"));
  auto test_obj = static_cast<TestQLObject*>(result.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre());
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre());
}

TEST_F(PyFuncTest, WrongArgumentConfigs) {
  var_table->Add("simplefunc",
                 FuncObject::Create("func", {"simple"}, {},
                                    /* has_variable_len_args */ false,
                                    /* has_variable_len_kwargs */ false,
                                    std::bind(&SimpleFunc, std::placeholders::_1,
                                              std::placeholders::_2, std::placeholders::_3),
                                    ast_visitor.get())
                     .ConsumeValueOrDie());
  EXPECT_COMPILER_ERROR(ParseExpression("simplefunc()"),
                        "func.* missing 1 required positional arguments 'simple'");
  EXPECT_COMPILER_ERROR(ParseExpression("simplefunc(1,2)"),
                        "func.* takes 1 arguments but 2 were given.");
  EXPECT_COMPILER_ERROR(ParseExpression("simplefunc(1,blah=2)"),
                        "func.* got an unexpected keyword argument 'blah'");
}

TEST_F(PyFuncTest, KwargExtraPosArg) {
  var_table->Add("kwarg",
                 FuncObject::Create("func", {"simple"}, {},
                                    /* has_variable_len_args */ false,
                                    /* has_variable_len_kwargs */ true,
                                    std::bind(&SimpleFunc, std::placeholders::_1,
                                              std::placeholders::_2, std::placeholders::_3),
                                    ast_visitor.get())
                     .ConsumeValueOrDie());
  EXPECT_COMPILER_ERROR(ParseExpression("kwarg(1,2)"),
                        "func.* takes 1 arguments but 2 were given.");
}

TEST_F(PyFuncTest, KwargsSupport) {
  var_table->Add("kwarg",
                 FuncObject::Create("func", {"simple"}, {},
                                    /* has_variable_len_args */ false,
                                    /* has_variable_len_kwargs */ true,
                                    std::bind(&SimpleFunc, std::placeholders::_1,
                                              std::placeholders::_2, std::placeholders::_3),
                                    ast_visitor.get())
                     .ConsumeValueOrDie());

  ASSERT_OK_AND_ASSIGN(auto result, ParseExpression("kwarg(123, blah1='blah2')"));
  auto test_obj = static_cast<TestQLObject*>(result.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre("blah1"));
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre("blah2"));
}

TEST_F(PyFuncTest, TestDefaultArgsCanBeAccessed) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();
  var_table->Add("defaultargs", func_obj);
  // The default argument should be parsed into the test object.
  auto result = ParseExpression("defaultargs()").ConsumeValueOrDie();
  auto test_objj = static_cast<TestQLObject*>(result.get());
  EXPECT_EQ(test_objj->value(), 1234);
}

// This test makes sure we use variable args.
TEST_F(PyFuncTest, TestVariableArgs) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}},
                         /* has_variable_args_len */ true,
                         /* has_kwargs */ false,
                         std::bind(&SimpleFuncForVarArgs, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();
  var_table->Add("vargs", func_obj);
  auto result = ParseExpression("vargs(123, 'str123', 'str321')").ConsumeValueOrDie();
  auto test_obj = static_cast<TestQLObject*>(result.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre("arg0", "arg1"));
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre("str123", "str321"));
}

// This test makes sure variable args and variable kwargs both work when used together.
TEST_F(PyFuncTest, VariableArgsAndKwargs) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"simple"}, {{"simple", "1234"}},
                         /* has_variable_args_len */ true,
                         /* has_kwargs */ true,
                         std::bind(&SimpleFuncForVarArgs, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  var_table->Add("vargs_kwargs", func_obj);
  auto result =
      ParseExpression("vargs_kwargs(123, 'str123', 'str321', kwarg1='str222', kwarg2='str333')")
          .ConsumeValueOrDie();
  auto test_obj = static_cast<TestQLObject*>(result.get());
  EXPECT_EQ(test_obj->value(), 123);

  EXPECT_THAT(test_obj->kwarg_names(), ElementsAre("arg0", "arg1", "kwarg1", "kwarg2"));
  EXPECT_THAT(test_obj->kwarg_values(), ElementsAre("str123", "str321", "str222", "str333"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
