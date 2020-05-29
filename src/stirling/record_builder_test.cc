#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/stirling/data_table.h"
#include "src/stirling/record_builder.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::ColWrapperSizeIs;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::IsEmpty;
using ::testing::StartsWith;

static constexpr DataElement kElements[] = {
    {"a", types::DataType::INT64, types::PatternType::GENERAL, ""},
    {"b", types::DataType::STRING, types::PatternType::GENERAL, ""},
    {"c", types::DataType::STRING, types::PatternType::GENERAL, ""},
};
static constexpr auto kTableSchema = DataTableSchema("abc_table", kElements);

TEST(RecordBuilder, StringMaxSize) {
  DataTable data_table(kTableSchema);

  std::string kLargeString(RecordBuilder<&kTableSchema>::kMaxStringBytes + 100, 'c');
  std::string kExpectedString(RecordBuilder<&kTableSchema>::kMaxStringBytes, 'c');

  RecordBuilder<&kTableSchema> r(&data_table);
  r.Append<r.ColIndex("a")>(1);
  r.Append<r.ColIndex("b")>("foo");
  r.Append<r.ColIndex("c")>(kLargeString);

  types::ColumnWrapperRecordBatch& record_batch = *data_table.ActiveRecordBatch();

  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));

  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), StartsWith(kExpectedString));
  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), EndsWith("[TRUNCATED]"));
}

TEST(RecordBuilder, UnfilledColNames) {
  DataTable data_table(kTableSchema);

  RecordBuilder<&kTableSchema> r(&data_table);
  EXPECT_THAT(r.UnfilledColNames(), ElementsAre("a", "b", "c"));

  r.Append<r.ColIndex("a")>(1);
  EXPECT_THAT(r.UnfilledColNames(), ElementsAre("b", "c"));
  r.Append<r.ColIndex("b")>("test");
  r.Append<r.ColIndex("c")>("test");
  EXPECT_THAT(r.UnfilledColNames(), IsEmpty());
}

}  // namespace stirling
}  // namespace pl
