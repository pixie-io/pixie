#include <memory>
#include <string>

#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/metadata_object.h"
#include "src/carnot/compiler/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

using MetadataObjectTest = QLObjectTest;

TEST_F(MetadataObjectTest, SubscriptWithString) {
  MemorySourceIR* src = MakeMemSource();
  auto metadata_or_s = MetadataObject::Create(src);
  ASSERT_OK(metadata_or_s);
  auto metadata = metadata_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(metadata->HasSubscriptMethod());
  std::shared_ptr<FuncObject> func = metadata->GetSubscriptMethod().ConsumeValueOrDie();

  auto func_result_or_s = func->Call(ArgMap{{}, {MakeString("service")}}, ast, ast_visitor.get());
  ASSERT_OK(func_result_or_s);

  auto func_result = func_result_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(func_result->type_descriptor().type() == QLObjectType::kExpr);
  auto metadata_expr = static_cast<ExprObject*>(func_result.get());
  ASSERT_TRUE(metadata_expr->HasNode());
  ASSERT_TRUE(Match(metadata_expr->node(), Metadata()));
  auto metadata_node = static_cast<MetadataIR*>(metadata_expr->node());
  EXPECT_EQ(metadata_node->name(), "service");
}

TEST_F(MetadataObjectTest, ErrorsOnSubscriptWithNonString) {
  MemorySourceIR* src = MakeMemSource();
  auto metadata_or_s = MetadataObject::Create(src);
  ASSERT_OK(metadata_or_s);
  auto metadata = metadata_or_s.ConsumeValueOrDie();

  ASSERT_TRUE(metadata->HasSubscriptMethod());
  std::shared_ptr<FuncObject> func = metadata->GetSubscriptMethod().ConsumeValueOrDie();

  auto func_result_or_s =
      func->Call(ArgMap{{}, {MakeList(MakeString("service"))}}, ast, ast_visitor.get());
  ASSERT_NOT_OK(func_result_or_s);
  EXPECT_THAT(func_result_or_s.status(), HasCompilerError("expected 'key' to be a str, got '.*'"));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
