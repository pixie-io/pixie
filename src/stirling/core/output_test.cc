#include "src/stirling/core/output.h"

#include "src/common/testing/testing.h"
#include "src/stirling/testing/dummy_table.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::DummyTableFixture;

namespace idx = ::px::stirling::testing::dummy_table_idx;

TEST(PrintRecordBatchTest, AllRecordsToString) {
  DummyTableFixture fixture;
  {
    auto r = fixture.record_builder();
    r.Append<idx::kInt64Idx>(0);
    r.Append<idx::kStringIdx>("test");
  }
  {
    auto r = fixture.record_builder();
    r.Append<idx::kInt64Idx>(0);
    r.Append<idx::kStringIdx>("test");
  }
  EXPECT_EQ(
      "[test] int64:[0] string:[test]\n"
      "[test] int64:[0] string:[test]\n",
      ToString("test", fixture.SchemaProto(), fixture.record_batch()));
}

}  // namespace stirling
}  // namespace px
