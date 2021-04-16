#include <gtest/gtest.h>

#include "src/stirling/core/connector_context.h"

#include "src/common/testing/testing.h"

using ::px::testing::TestFilePath;
using ::testing::UnorderedElementsAre;

namespace px {
namespace stirling {

TEST(ListUPIDs, Basic) {
  const std::filesystem::path proc_path = TestFilePath("src/common/system/testdata/proc");
  EXPECT_THAT(ListUPIDs(proc_path),
              UnorderedElementsAre(md::UPID{0, 123, 14329}, md::UPID{0, 1, 13},
                                   md::UPID{0, 456, 17594622}, md::UPID{0, 789, 46120203}));
}

}  // namespace stirling
}  // namespace px
