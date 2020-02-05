#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/compiler/objects/flags_object.h"
#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/objects/qlobject.h"
#include "src/carnot/compiler/objects/test_utils.h"
#include "src/carnot/compiler/objects/type_object.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::testing::ElementsAre;

class FlagsObjectTest : public QLObjectTest {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    FlagValue flag;
    flag.set_flag_name("foo");
    EXPECT_OK(MakeString("non-default")->ToProto(flag.mutable_flag_value()));
    flags_obj_ = FlagsObject::Create(graph.get(), {flag}).ConsumeValueOrDie();
  }

  StatusOr<std::shared_ptr<ExprObject>> GetFlagSubscript(const std::string& flag_name) {
    ArgMap args = MakeArgMap({}, {MakeString(flag_name)});

    PL_ASSIGN_OR_RETURN(auto get_method, flags_obj_->GetSubscriptMethod());
    PL_ASSIGN_OR_RETURN(auto ql_object, get_method->Call(args, ast, ast_visitor.get()));
    EXPECT_TRUE(QLObjectType::kExpr == ql_object->type_descriptor().type());
    return std::static_pointer_cast<ExprObject>(ql_object);
  }

  StatusOr<std::shared_ptr<ExprObject>> GetFlagAttribute(std::string_view flag_name) {
    PL_ASSIGN_OR_RETURN(auto ql_object, flags_obj_->GetAttribute(ast, flag_name));
    EXPECT_TRUE(QLObjectType::kExpr == ql_object->type_descriptor().type());
    return std::static_pointer_cast<ExprObject>(ql_object);
  }

  Status CallParseFlags() {
    ArgMap args;
    PL_ASSIGN_OR_RETURN(auto parse_method, flags_obj_->GetMethod("parse"));
    PL_ASSIGN_OR_RETURN(auto ql_object, parse_method->Call(args, ast, ast_visitor.get()));
    EXPECT_TRUE(QLObjectType::kNone == ql_object->type_descriptor().type());
    EXPECT_FALSE(ql_object->HasNode());
    return Status::OK();
  }

  Status CallRegisterFlag(const std::string& name, IRNodeType type, const std::string& descr,
                          ExpressionIR* defaultval) {
    std::vector<QLObjectPtr> args;
    args.push_back(QLObject::FromIRNode(MakeString(name)).ConsumeValueOrDie());
    std::vector<NameToNode> kwargs;
    kwargs.push_back(
        {"type", std::static_pointer_cast<QLObject>(TypeObject::Create(type).ConsumeValueOrDie())});
    kwargs.push_back({"description", QLObject::FromIRNode(MakeString(descr)).ConsumeValueOrDie()});
    kwargs.push_back({"default", QLObject::FromIRNode(defaultval).ConsumeValueOrDie()});
    ArgMap argmap{kwargs, args};

    PL_ASSIGN_OR_RETURN(auto register_method, flags_obj_->GetCallMethod());
    PL_ASSIGN_OR_RETURN(auto ql_object, register_method->Call(argmap, ast, ast_visitor.get()));
    EXPECT_TRUE(QLObjectType::kNone == ql_object->type_descriptor().type());
    EXPECT_FALSE(ql_object->HasNode());
    return Status::OK();
  }

  std::shared_ptr<FlagsObject> flags_obj_;
};

TEST_F(FlagsObjectTest, TestBasicAttribute) {
  ASSERT_OK(CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default")));
  ASSERT_OK(CallRegisterFlag("bar", IRNodeType::kInt, "an int", MakeInt(123)));
  ASSERT_OK(CallParseFlags());

  // Get non-default value
  auto res_or_s = GetFlagAttribute("foo");
  ASSERT_OK(res_or_s);
  auto expr = res_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(expr->HasNode());
  EXPECT_EQ(IRNodeType::kString, expr->node()->type());
  auto strval = static_cast<StringIR*>(expr->node());
  EXPECT_EQ("non-default", strval->str());

  // Get default value
  res_or_s = GetFlagAttribute("bar");
  ASSERT_OK(res_or_s);
  expr = res_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(expr->HasNode());
  EXPECT_EQ(IRNodeType::kInt, expr->node()->type());
  auto intval = static_cast<IntIR*>(expr->node());
  EXPECT_EQ(123, intval->val());
}

TEST_F(FlagsObjectTest, TestBasicSubscript) {
  ASSERT_OK(CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default")));
  ASSERT_OK(CallRegisterFlag("bar", IRNodeType::kInt, "an int", MakeInt(123)));
  ASSERT_OK(CallParseFlags());

  // Get non-default value
  auto res_or_s = GetFlagSubscript("foo");
  ASSERT_OK(res_or_s);
  auto expr = res_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(expr->HasNode());
  EXPECT_EQ(IRNodeType::kString, expr->node()->type());
  auto strval = static_cast<StringIR*>(expr->node());
  EXPECT_EQ("non-default", strval->str());

  // Get default value
  res_or_s = GetFlagSubscript("bar");
  ASSERT_OK(res_or_s);
  expr = res_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(expr->HasNode());
  EXPECT_EQ(IRNodeType::kInt, expr->node()->type());
  auto intval = static_cast<IntIR*>(expr->node());
  EXPECT_EQ(123, intval->val());
}

TEST_F(FlagsObjectTest, TestErrorOnMissingFlag) {
  ASSERT_OK(CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default")));
  ASSERT_OK(CallParseFlags());

  auto s = GetFlagSubscript("bar");
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(), HasCompilerError("Flag bar not registered"));
}

TEST_F(FlagsObjectTest, TestErrorOnMismatchedType) {
  auto s = CallRegisterFlag("foo", IRNodeType::kInt, "an int", MakeString("default"));
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(),
              HasCompilerError("For flag foo expected type Int but received type String"));
}

// TODO(nserrino): Support compile time expressions
TEST_F(FlagsObjectTest, TestErrorOnExpr) {
  auto s = CallRegisterFlag("foo", IRNodeType::kInt, "an int", MakeAddFunc(MakeInt(2), MakeInt(2)));
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(),
              HasCompilerError("Value for 'default' in px.flags must be a constant literal"));
}

TEST_F(FlagsObjectTest, TestErrorOnReregister) {
  ASSERT_OK(CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default")));
  auto s = CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default"));
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(), HasCompilerError("Flag foo already registered"));
}

TEST_F(FlagsObjectTest, TestErrorOnGetBeforeParse) {
  ASSERT_OK(CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default")));
  auto s = GetFlagSubscript("foo");
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(),
              HasCompilerError("Cannot access flags before px.flags.parse.* has been called"));
}

TEST_F(FlagsObjectTest, TestErrorOnReparse) {
  ASSERT_OK(CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default")));
  ASSERT_OK(CallRegisterFlag("bar", IRNodeType::kInt, "an int", MakeInt(123)));
  ASSERT_OK(CallParseFlags());
  auto s = CallParseFlags();
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(), HasCompilerError("px.flags.parse.* must only be called once"));
}

TEST_F(FlagsObjectTest, TestErrorDefineAfterParse) {
  ASSERT_OK(CallParseFlags());
  auto s = CallRegisterFlag("foo", IRNodeType::kString, "a string", MakeString("default"));
  ASSERT_NOT_OK(s);
  EXPECT_THAT(s.status(),
              HasCompilerError("Could not add flag foo after px.flags.parse.* has been called"));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
