#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/compiler/objects/funcobject.h"
#include "src/carnot/compiler/objects/test_utils.h"

namespace pl {
namespace carnot {
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

  TestQLObject() : QLObject(TestQLObjectType) {
    std::shared_ptr<FuncObject> func_obj =
        FuncObject::Create("func", {}, {}, /* has_variable_len_args */ false,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&TestQLObject::SimpleFunc, this, std::placeholders::_1,
                                     std::placeholders::_2))
            .ConsumeValueOrDie();
    AddMethod("func", func_obj);
  }

  StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr&, const ParsedArgs&) {
    auto out_obj = std::make_shared<TestQLObject>();
    return StatusOr<QLObjectPtr>(out_obj);
  }
};

TEST_F(QLObjectTest, GetMethod) {
  auto test_object = std::make_shared<TestQLObject>();
  // No attributes in the TestQLObject, so should return the method
  EXPECT_TRUE(test_object->HasMethod("func"));
  auto attr_or_s = test_object->GetMethod("func");
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);
  EXPECT_FALSE(func_attr->HasNode());

  auto out_object = func_attr->Call(ArgMap{}, ast, ast_visitor.get()).ConsumeValueOrDie();
  // Just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject::TestQLObjectType.name());
}

// Gets the method via an attribute.
TEST_F(QLObjectTest, GetMethodAsAttribute) {
  auto test_object = std::make_shared<TestQLObject>();
  // No attributes in the TestQLObject, so should return the method
  EXPECT_TRUE(test_object->HasAttribute("func"));
  auto attr_or_s = test_object->GetAttribute(ast, "func");
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);
  EXPECT_FALSE(func_attr->HasNode());

  auto func = static_cast<FuncObject*>(func_attr.get());
  auto out_object = func->Call(ArgMap{}, ast, ast_visitor.get()).ConsumeValueOrDie();
  // Just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject::TestQLObjectType.name());
}

// Attribute not found in either impl or the methods.
TEST_F(QLObjectTest, AttributeNotFound) {
  auto test_object = std::make_shared<TestQLObject>();
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
  auto test_object = std::make_shared<TestQLObject>();
  // No attributes in the TestQLObject, so should return the method
  std::string attr_name = "bar";
  auto attr_or_s = test_object->GetMethod(attr_name);
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status().msg(),
              ContainsRegex(absl::Substitute("'$0'.* has no attribute .*$1",
                                             test_object->type_descriptor().name(), attr_name)));
}

TEST_F(QLObjectTest, NotSubscriptable) {
  auto test_object = std::make_shared<TestQLObject>();
  // No attributes in the TestQLObject, so should return the method
  auto attr_or_s = test_object->GetSubscriptMethod();
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status().msg(),
              ContainsRegex(absl::Substitute("'$0'.* is not subscriptable",
                                             test_object->type_descriptor().name())));
}

TEST_F(QLObjectTest, NotCallable) {
  auto test_object = std::make_shared<TestQLObject>();
  // No attributes in the TestQLObject, so should return the method
  auto attr_or_s = test_object->GetCallMethod();
  ASSERT_NOT_OK(attr_or_s);
  EXPECT_THAT(attr_or_s.status().msg(),
              ContainsRegex(absl::Substitute("'$0'.* is not callable",
                                             test_object->type_descriptor().name())));
}

class TestQLObject2 : public QLObject {
 public:
  static constexpr TypeDescriptor TestQLObjectType = {
      /* name */ "TestQLObject2",
      /* type */ QLObjectType::kMisc,
  };

  TestQLObject2() : QLObject(TestQLObjectType) {
    std::shared_ptr<FuncObject> func_obj =
        FuncObject::Create(kSubscriptMethodName, {"key"}, {}, /* has_variable_len_args */ false,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&TestQLObject2::SimpleFunc, this, std::placeholders::_1,
                                     std::placeholders::_2))
            .ConsumeValueOrDie();
    AddSubscriptMethod(func_obj);
    AddCallMethod(func_obj);
    attributes_.emplace(kSpecialAttr);
  }

  StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr&, const ParsedArgs&) {
    auto out_obj = std::make_shared<TestQLObject2>();
    return StatusOr<QLObjectPtr>(out_obj);
  }

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr&,
                                         const std::string& name) const override {
    DCHECK(HasNonMethodAttribute(name));
    auto out_obj = std::make_shared<TestQLObject2>();
    out_obj->SetName(name);
    return StatusOr<QLObjectPtr>(out_obj);
  }

  void SetName(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  inline static constexpr char kSpecialAttr[] = "foobar";

 private:
  std::string name_;
};

TEST_F(QLObjectTest, GetSubscriptMethod) {
  auto test_object = std::make_shared<TestQLObject2>();
  // No attributes in the TestQLObject, so should return the method
  EXPECT_TRUE(test_object->HasSubscriptMethod());
  auto attr_or_s = test_object->GetSubscriptMethod();
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);
  EXPECT_FALSE(func_attr->HasNode());

  auto out_object =
      func_attr->Call(ArgMap{{}, {MakeInt(10)}}, ast, ast_visitor.get()).ConsumeValueOrDie();
  // just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject2::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject2::TestQLObjectType.name());
}

// Gets the method via an attribute.
TEST_F(QLObjectTest, GetCallMethod) {
  auto test_object = std::make_shared<TestQLObject2>();
  EXPECT_TRUE(test_object->HasCallMethod());
  auto attr_or_s = test_object->GetCallMethod();
  ASSERT_OK(attr_or_s);
  auto func_attr = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_attr->type_descriptor().type() == QLObjectType::kFunction);
  EXPECT_FALSE(func_attr->HasNode());

  auto out_object =
      func_attr->Call(ArgMap{{}, {MakeInt(10)}}, ast, ast_visitor.get()).ConsumeValueOrDie();
  // just validate that the correct thing happened.
  ASSERT_TRUE(out_object->type_descriptor().type() == TestQLObject2::TestQLObjectType.type());
  ASSERT_TRUE(out_object->type_descriptor().name() == TestQLObject2::TestQLObjectType.name());
}

// Gets attribute that's not a method.
TEST_F(QLObjectTest, GetAttribute) {
  auto test_object = std::make_shared<TestQLObject2>();
  std::string attr = TestQLObject2::kSpecialAttr;
  EXPECT_TRUE(test_object->HasAttribute(attr));
  auto attr_or_s = test_object->GetAttribute(ast, attr);
  ASSERT_OK(attr_or_s);
  auto attr_result = attr_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(attr_result->type_descriptor().type() == TestQLObject2::TestQLObjectType.type());
  ASSERT_TRUE(attr_result->type_descriptor().name() == TestQLObject2::TestQLObjectType.name());

  auto object = static_cast<TestQLObject2*>(attr_result.get());
  // Just verify that the attribute method works as expected.
  EXPECT_EQ(object->name(), attr);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
