#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/stirling/data_table.h"
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
    {"a", "", types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
    {"b", "", types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
    {"c", "", types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
};
static constexpr auto kTableSchema = DataTableSchema("abc_table", kElements);

TEST(RecordBuilder, StringMaxSize) {
  DataTable data_table(kTableSchema);

  constexpr size_t kMaxStringBytes = 512;

  std::string kLargeString(kMaxStringBytes + 100, 'c');
  std::string kExpectedString(kMaxStringBytes, 'c');

  DataTable::RecordBuilder<&kTableSchema> r(&data_table);
  r.Append<r.ColIndex("a"), kMaxStringBytes>(1);
  r.Append<r.ColIndex("b"), kMaxStringBytes>("foo");
  r.Append<r.ColIndex("c"), kMaxStringBytes>(kLargeString);

  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_EQ(tablets.size(), 1);
  types::ColumnWrapperRecordBatch& record_batch = tablets[0].records;

  ASSERT_THAT(record_batch, Each(ColWrapperSizeIs(1)));

  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), StartsWith(kExpectedString));
  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), EndsWith("[TRUNCATED]"));
}

TEST(RecordBuilder, UnfilledColNames) {
  DataTable data_table(kTableSchema);

  DataTable::RecordBuilder<&kTableSchema> r(&data_table);
  EXPECT_THAT(r.UnfilledColNames(), ElementsAre("a", "b", "c"));

  r.Append<r.ColIndex("a")>(1);
  EXPECT_THAT(r.UnfilledColNames(), ElementsAre("b", "c"));
  r.Append<r.ColIndex("b")>("test");
  r.Append<r.ColIndex("c")>("test");
  EXPECT_THAT(r.UnfilledColNames(), IsEmpty());
}

}  // namespace stirling
}  // namespace pl
