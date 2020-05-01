#include "src/stirling/http2/frame.h"

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace http2 {

TEST(NVMapTest, DebugString) {
  NVMap nv_map{{"a", "b"}, {"a", "bb"}};
  EXPECT_EQ("a:b, a:bb", nv_map.DebugString());
}

}  // namespace http2
}  // namespace stirling
}  // namespace pl
