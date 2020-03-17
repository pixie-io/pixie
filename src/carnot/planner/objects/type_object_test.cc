#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/type_object.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
class TypeObjectTest : public QLObjectTest {};
TEST_F(TypeObjectTest, NodeMatches) {
  std::shared_ptr<TypeObject> type =
      TypeObject::Create(IRNodeType::kString, ast_visitor.get()).ConsumeValueOrDie();
  EXPECT_OK(type->NodeMatches(MakeString("blah")));
  auto match = type->NodeMatches(MakeInt(123));
  EXPECT_NOT_OK(match);
  EXPECT_THAT(match.status(), HasCompilerError("Expected 'string', received 'int64'"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
