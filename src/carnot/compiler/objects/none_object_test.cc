#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/compiler/objects/none_object.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::testing::ElementsAre;
class NoneObjectTest : public OperatorTests {};
TEST_F(NoneObjectTest, TestNoMethodsWork) {
  std::shared_ptr<NoneObject> none = std::make_shared<NoneObject>();
  auto status = none->GetMethod("agg");
  ASSERT_NOT_OK(status);
  EXPECT_EQ("'None' object has no attribute 'agg'", status.status().msg());
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
