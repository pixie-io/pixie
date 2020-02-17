#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/objects/type_object.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
class TypeObjectTest : public OperatorTests {};
TEST_F(TypeObjectTest, NodeMatches) {
  std::shared_ptr<TypeObject> type = TypeObject::Create(IRNodeType::kString).ConsumeValueOrDie();
  EXPECT_OK(type->NodeMatches(MakeString("blah")));
  auto match = type->NodeMatches(MakeInt(123));
  EXPECT_NOT_OK(match);
  EXPECT_THAT(match.status(), HasCompilerError("Expected 'String', received 'Int'"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
