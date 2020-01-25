#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/compiler/objects/type_object.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
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
}  // namespace carnot
}  // namespace pl
