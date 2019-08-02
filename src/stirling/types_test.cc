#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/stirling/types.h"

namespace pl {
namespace stirling {

using types::DataType;
using types::PatternType;

TEST(DataElementTest, data_element_proto_getters_test) {
  DataElement element("user_percentage", DataType::FLOAT64, PatternType::METRIC_GAUGE);

  EXPECT_EQ("user_percentage", std::string(element.name().data()));
  EXPECT_EQ(DataType::FLOAT64, element.type());

  stirlingpb::Element element_pb;
  element_pb = element.ToProto();
  EXPECT_EQ("user_percentage", element_pb.name());
  EXPECT_EQ(DataType::FLOAT64, element_pb.type());
}

TEST(DataTableSchemaTest, table_schema_proto_getters_test) {
  DataElement elements[] = {
      {"time_", types::DataType::TIME64NS, types::PatternType::METRIC_COUNTER},
      {"a", types::DataType::INT64, types::PatternType::GENERAL},
      {"b", types::DataType::INT64, types::PatternType::GENERAL_ENUM},
      {"c", types::DataType::INT64, types::PatternType::GENERAL},
      {"d", types::DataType::INT64, types::PatternType::GENERAL},
      {"e", types::DataType::FLOAT64, types::PatternType::GENERAL},
  };
  auto table_schema = DataTableSchema("table", elements);

  EXPECT_EQ("table", table_schema.name());
  EXPECT_EQ(false, table_schema.tabletized());
  EXPECT_EQ(6, table_schema.elements().size());
  EXPECT_EQ("c", table_schema.elements()[3].name());

  stirlingpb::TableSchema table_schema_pb;
  table_schema_pb = table_schema.ToProto();
  EXPECT_EQ("table", table_schema_pb.name());
  EXPECT_EQ(false, table_schema_pb.tabletized());
  EXPECT_EQ(6, table_schema_pb.elements().size());
  EXPECT_EQ("c", table_schema_pb.elements(3).name());
}

}  // namespace stirling
}  // namespace pl
