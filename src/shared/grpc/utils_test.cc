#include "src/shared/grpc/utils.h"

#include "src/common/testing/testing.h"

namespace pl {
namespace grpc {

TEST(MethodPathTest, ResutlsAreAsExpected) { EXPECT_EQ("a.b.c.foo", MethodPath("/a.b.c/foo")); }

}  // namespace grpc
}  // namespace pl
