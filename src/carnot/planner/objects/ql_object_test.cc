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

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ContainsRegex;
using ::testing::ElementsAre;

class TestQLObject;

class TestQLObject : public QLObject {
 public:
  static constexpr TypeDescriptor TestQLObjectType = {
      /* name */ "TestQLObject",
      /* type */ QLObjectType::kMisc,
  };

  TestQLObject(const pypa::AstPtr& ast, ASTVisitor* visitor)
      : QLObject(TestQLObjectType, ast, visitor) {
    std::shared_ptr<FuncObject> func_obj =
        FuncObject::Create("func", {}, {}, /* has_variable_len_args */ false,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&TestQLObject::SimpleFunc, this, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3),
                           visitor)
            .ConsumeValueOrDie();
    AddMethod("func", func_obj);
  }

  StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr& ast, const ParsedArgs&,
                                   ASTVisitor* visitor) {
    auto out_obj = std::make_shared<TestQLObject>(ast, visitor);
    return StatusOr<QLObjectPtr>(out_obj);
  }
};

TEST_F(QLObjectTest, GetMethod) {
  auto test_object = std::make_shared<TestQLObject>(ast, ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  EXPECT_TRUE(test_object->HasMethod("func"));
  auto attr_or_s = test_object->GetMethod("func");
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);

  auto out_object = func_attr->Call(ArgMap{}, ast).ConsumeValueOrDie();
  // Just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject::TestQLObjectType.name());
}

// Gets the method via an attribute.
TEST_F(QLObjectTest, GetMethodAsAttribute) {
  auto test_object = std::make_shared<TestQLObject>(ast, ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  EXPECT_TRUE(test_object->HasAttribute("func"));
  auto attr_or_s = test_object->GetAttribute(ast, "func");
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);

  auto func = static_cast<FuncObject*>(func_attr.get());
  auto out_object = func->Call(ArgMap{}, ast).ConsumeValueOrDie();
  // Just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject::TestQLObjectType.name());
}

// Attribute not found in either impl or the methods.
TEST_F(QLObjectTest, AttributeNotFound) {
  auto test_object = std::make_shared<TestQLObject>(ast, ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  std::string attr_name = "bar";
  auto attr_or_s = test_object->GetAttribute(ast, attr_name);
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status(),
              HasCompilerError("'$0'.* has no attribute .*$1",
                               test_object->type_descriptor().name(), attr_name));
}

// Method not found.
TEST_F(QLObjectTest, MethodNotFound) {
  auto test_object = std::make_shared<TestQLObject>(ast, ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  std::string attr_name = "bar";
  auto attr_or_s = test_object->GetMethod(attr_name);
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status(),
              HasCompilerError("'$0'.* has no attribute .*$1",
                               test_object->type_descriptor().name(), attr_name));
}

TEST_F(QLObjectTest, NotSubscriptable) {
  auto test_object = std::make_shared<TestQLObject>(ast, ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  auto attr_or_s = test_object->GetSubscriptMethod();
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status(), HasCompilerError("'$0'.* is not subscriptable",
                                                   test_object->type_descriptor().name()));
}

TEST_F(QLObjectTest, NotCallable) {
  auto test_object = std::make_shared<TestQLObject>(ast, ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  auto attr_or_s = test_object->GetCallMethod();
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status(),
              HasCompilerError("'$0'.* is not callable", test_object->type_descriptor().name()));
}

class TestQLObject2 : public QLObject {
 public:
  static constexpr TypeDescriptor TestQLObjectType = {
      /* name */ "TestQLObject2",
      /* type */ QLObjectType::kMisc,
  };

  explicit TestQLObject2(ASTVisitor* visitor) : QLObject(TestQLObjectType, visitor) {
    std::shared_ptr<FuncObject> func_obj =
        FuncObject::Create(kSubscriptMethodName, {"key"}, {}, /* has_variable_len_args */ false,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&TestQLObject2::SimpleFunc, this, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3),
                           visitor)
            .ConsumeValueOrDie();
    AddSubscriptMethod(func_obj);
    AddCallMethod(func_obj);
  }

  StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr&, const ParsedArgs&, ASTVisitor* visitor) {
    auto out_obj = std::make_shared<TestQLObject2>(visitor);
    return StatusOr<QLObjectPtr>(out_obj);
  }

  bool HasNonMethodAttribute(std::string_view name) const override { return name == kSpecialAttr; }

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr&,
                                         std::string_view name) const override {
    DCHECK(HasNonMethodAttribute(name));
    auto out_obj = std::make_shared<TestQLObject2>(ast_visitor());
    out_obj->SetName(std::string(name));
    return StatusOr<QLObjectPtr>(out_obj);
  }

  void SetName(const std::string& name) { internal_name_ = name; }
  const std::string& internal_name() const { return internal_name_; }

  inline static constexpr char kSpecialAttr[] = "foobar";

 private:
  std::string internal_name_;
};

TEST_F(QLObjectTest, GetSubscriptMethod) {
  auto test_object = std::make_shared<TestQLObject2>(ast_visitor.get());
  // No attributes in the TestQLObject, so should return the method
  EXPECT_TRUE(test_object->HasSubscriptMethod());
  auto attr_or_s = test_object->GetSubscriptMethod();
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);

  auto out_object = func_attr->Call(MakeArgMap({}, {MakeInt(10)}), ast).ConsumeValueOrDie();
  // just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject2::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject2::TestQLObjectType.name());
}

// Gets the method via an attribute.
TEST_F(QLObjectTest, GetCallMethod) {
  auto test_object = std::make_shared<TestQLObject2>(ast_visitor.get());
  EXPECT_TRUE(test_object->HasCallMethod());
  auto attr_or_s = test_object->GetCallMethod();
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);

  auto out_object = func_attr->Call(MakeArgMap({}, {MakeInt(10)}), ast).ConsumeValueOrDie();
  // just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject2::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject2::TestQLObjectType.name());
}

// Gets attribute that's not a method.
TEST_F(QLObjectTest, GetAttribute) {
  auto test_object = std::make_shared<TestQLObject2>(ast_visitor.get());
  std::string attr = TestQLObject2::kSpecialAttr;
  EXPECT_TRUE(test_object->HasAttribute(attr));
  auto attr_or_s = test_object->GetAttribute(ast, attr);
  ASSERT_OK(attr_or_s);
  auto attr_result = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(attr_result->type_descriptor().type() == TestQLObject2::TestQLObjectType.type());
  ASSERT_TRUE(attr_result->type_descriptor().name() == TestQLObject2::TestQLObjectType.name());

  auto object = static_cast<TestQLObject2*>(attr_result.get());
  // Just verify that the attribute method works as expected.
  EXPECT_EQ(object->internal_name(), attr);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
