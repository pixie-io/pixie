#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/stirling/utils/task_struct_offset_resolver/task_struct_offset_resolver.h"

namespace pl {
namespace stirling {
namespace utils {

TEST(ResolveTaskStructOffsets, Basic) {
  ASSERT_OK_AND_ASSIGN(TaskStructOffsets offsets, ResolveTaskStructOffsets());

  EXPECT_NE(offsets.real_start_time, 0);
  EXPECT_NE(offsets.group_leader, 0);
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
