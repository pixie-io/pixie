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
  DataElement element("user_percentage", "", DataType::FLOAT64, types::SemanticType::ST_NONE,
                      PatternType::METRIC_GAUGE);

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
      {"time_", "", types::DataType::TIME64NS, types::SemanticType::ST_NONE,
       types::PatternType::METRIC_COUNTER},
      {"a", "", types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      {"b", "", types::DataType::INT64, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL_ENUM, &enum_decoder},
      {"c", "", types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      {"d", "", types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
      {"e", "", types::DataType::FLOAT64, types::SemanticType::ST_NONE,
       types::PatternType::GENERAL},
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

TEST(DynamicDataTableSchemaTest, generate) {
  constexpr std::string_view kOutputStruct = R"(
  name: "out_table_value_t"
  fields {
    name: "tgid_"
    type: INT32
  }
  fields {
    name: "tgid_start_time_"
    type: UINT64
  }
  fields {
    name: "goid_"
    type: INT64
  }
  fields {
    name: "time_"
    type: UINT64
  }
  fields {
    name: "arg0"
    type: INT
  }
  fields {
    name: "arg1"
    type: BOOL
  }
  fields {
    name: "arg2"
    type: BOOL
  }
)";

  dynamic_tracing::BCCProgram::PerfBufferSpec output_spec;
  output_spec.name = "out_table";
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(std::string(kOutputStruct),
                                                            &output_spec.output));

  std::unique_ptr<DynamicDataTableSchema> table_schema_ptr =
      DynamicDataTableSchema::Create(output_spec);

  const DataTableSchema& table_schema = table_schema_ptr->Get();

  EXPECT_EQ(table_schema.name(), "out_table");
  ASSERT_EQ(table_schema.elements().size(), 6);
  EXPECT_EQ(table_schema.tabletized(), false);
  EXPECT_EQ(table_schema.ColIndex("upid"), 0);
  EXPECT_EQ(table_schema.ColIndex("arg2"), 5);
  EXPECT_EQ(table_schema.elements()[1].name(), "goid_");
  EXPECT_EQ(table_schema.elements()[5].name(), "arg2");

  // There's a hack to convert any column with name "time_" to TIME64NS. Check that.
  EXPECT_EQ(table_schema.elements()[table_schema.ColIndex("time_")].type(), types::TIME64NS);
}

TEST(DynamicDataTableSchemaTest, GenerateForBPFTrace) {
  constexpr std::string_view kBPFTraceWithSameFields = R"(
    program: "test"
    outputs {
      name: "tgid_"
      type: INT32
    }
    outputs {
      name: "tgid_start_time_"
      type: UINT64
    }
    outputs {
      name: "goid_"
      type: INT64
    }
    outputs {
      name: "time_"
      type: UINT64
    }
    outputs {
      name: "arg0"
      type: INT
    }
    outputs {
      name: "arg1"
      type: BOOL
    }
    outputs {
      name: "arg2"
      type: BOOL
    }
  )";

  dynamic_tracing::ir::logical::BPFTrace bpftrace;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(std::string(kBPFTraceWithSameFields),
                                                            &bpftrace));
  auto data_table_schema = DynamicDataTableSchema::Create("out_table", bpftrace);

  const DataTableSchema& table_schema = data_table_schema->Get();

  EXPECT_EQ(table_schema.name(), "out_table");
  ASSERT_EQ(table_schema.elements().size(), 6);
  EXPECT_EQ(table_schema.tabletized(), false);
  EXPECT_EQ(table_schema.ColIndex("upid"), 0);
  EXPECT_EQ(table_schema.ColIndex("arg2"), 5);
  EXPECT_EQ(table_schema.elements()[1].name(), "goid_");
  EXPECT_EQ(table_schema.elements()[5].name(), "arg2");

  // There's a hack to convert any column with name "time_" to TIME64NS. Check that.
  EXPECT_EQ(table_schema.elements()[table_schema.ColIndex("time_")].type(), types::TIME64NS);
}

}  // namespace stirling
}  // namespace pl
