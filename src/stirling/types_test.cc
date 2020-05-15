#include <gtest/gtest.h>
#include "src/common/testing/testing.h"

#include "src/stirling/types.h"

using ::pl::types::DataType;
using ::pl::types::PatternType;

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace pl {
namespace stirling {

TEST(DataElementTest, data_element_proto_getters_test) {
  DataElement element("user_percentage", DataType::FLOAT64, PatternType::METRIC_GAUGE, "");

  EXPECT_EQ("user_percentage", std::string(element.name().data()));
  EXPECT_EQ(DataType::FLOAT64, element.type());

  stirlingpb::Element element_pb;
  element_pb = element.ToProto();
  EXPECT_EQ("user_percentage", element_pb.name());
  EXPECT_EQ(DataType::FLOAT64, element_pb.type());
}

TEST(DataTableSchemaTest, table_schema_proto_getters_test) {
  enum class BEnum : int64_t { kLow, kMed, kHigh = 99 };
  const std::map<int64_t, std::string_view> enum_decoder = EnumDefToMap<BEnum>();
  DataElement elements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER, ""},
      {"a", types::DataType::INT64, types::PatternType::GENERAL, ""},
      {"b", types::DataType::INT64, types::PatternType::GENERAL_ENUM, "", &enum_decoder},
      {"c", types::DataType::INT64, types::PatternType::GENERAL, ""},
      {"d", types::DataType::INT64, types::PatternType::GENERAL, ""},
      {"e", types::DataType::FLOAT64, types::PatternType::GENERAL, ""},
  };
  auto table_schema = DataTableSchema("table", elements);

  EXPECT_EQ(1, table_schema.ColIndex("a"));
  EXPECT_EQ("a", table_schema.ColName(1));
  EXPECT_EQ("table", table_schema.name());
  EXPECT_EQ(false, table_schema.tabletized());
  EXPECT_EQ(6, table_schema.elements().size());
  EXPECT_EQ("c", table_schema.elements()[3].name());
  EXPECT_EQ(nullptr, table_schema.elements()[1].decoder());
  EXPECT_NE(nullptr, table_schema.elements()[2].decoder());

  stirlingpb::TableSchema table_schema_pb;
  table_schema_pb = table_schema.ToProto();
  EXPECT_EQ("table", table_schema_pb.name());
  EXPECT_EQ(false, table_schema_pb.tabletized());
  EXPECT_EQ(6, table_schema_pb.elements().size());
  EXPECT_EQ("c", table_schema_pb.elements(3).name());
  EXPECT_THAT(table_schema_pb.elements(3).decoder(), IsEmpty());
  EXPECT_THAT(table_schema_pb.elements(2).decoder(),
              UnorderedElementsAre(Pair(0, "kLow"), Pair(1, "kMed"), Pair(99, "kHigh")));
}

}  // namespace stirling
}  // namespace pl
