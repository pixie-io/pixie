#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ElementsAre;
class NoneObjectTest : public QLObjectTest {};
TEST_F(NoneObjectTest, TestNoMethodsWork) {
  std::shared_ptr<NoneObject> none = std::make_shared<NoneObject>(ast_visitor.get());
  auto status = none->GetMethod("agg");
  ASSERT_NOT_OK(status);
  EXPECT_EQ("'None' object has no attribute 'agg'", status.status().msg());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
