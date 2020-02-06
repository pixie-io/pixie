#include <cpptoml.h>
#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/logical_planner/logical_planner.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/carnot/compiler/plannerpb/query_flags.pb.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"
#include "src/shared/schema/utils.h"
#include "src/stirling/stirling.h"
#include "src/table_store/schema/schema.h"

namespace pl {
namespace carnot {
namespace compiler {

using ::pl::table_store::schema::Relation;
using ::pl::testing::proto::EqualsProto;
using planpb::testutils::CompareLogicalPlans;
using ::testing::_;
using ::testing::ContainsRegex;

class PresetQueriesTest : public ::testing::Test {
 protected:
  void SetUpRegistryInfo() {
    info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie();
    udf_info_ = info_->info_pb();
  }

  void SetUp() override {
    SetUpRegistryInfo();
    auto rel_map = std::make_unique<RelationMap>();
    absl::flat_hash_map<std::string, Relation> absl_rel_map;

    // Get the production relations from Stirling
    auto stirling = stirling::Stirling::Create(stirling::CreateProdSourceRegistry());
    stirling::stirlingpb::Publish publish_pb;
    stirling->GetPublishProto(&publish_pb);
    auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);
    auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);

    for (const auto& rel_info : relation_info_vec) {
      rel_map->emplace(rel_info.name, rel_info.relation);
      absl_rel_map[rel_info.name] = rel_info.relation;
    }

    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);
    compiler_ = Compiler();

    EXPECT_OK(table_store::schema::Schema::ToProto(&schema_, absl_rel_map));
    ParsePresetQueries();
  }

  void ParsePresetQueries() {
    std::shared_ptr<cpptoml::table> tomls = cpptoml::parse_file(tomlpath_);
    EXPECT_TRUE(tomls != nullptr);
    EXPECT_TRUE(tomls->contains("queries"));

    auto querypairs = tomls->get_array_of<cpptoml::array>("queries");
    for (size_t i = 0; i < querypairs->size(); ++i) {
      auto querypair = (*querypairs)[i]->get_array_of<std::string>();
      EXPECT_EQ(2, querypair->size());
      preset_queries_.emplace((*querypair)[0], (*querypair)[1]);
    }
  }

  // Using map so that test order is deterministic.
  std::map<std::string, std::string> preset_queries_;
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  Compiler compiler_;
  table_store::schemapb::Schema schema_;
  Relation cgroups_relation_;
  const std::string tomlpath_ = "src/ui/src/containers/vizier/preset-queries.toml";
  udfspb::UDFInfo udf_info_;
};

TEST_F(PresetQueriesTest, PresetQueries) {
  // Test compilation
  for (const auto& query : preset_queries_) {
    auto plan = compiler_.Compile(query.second, compiler_state_.get(), /*query_flags*/ {});
    ASSERT_OK(plan) << "Query '" << query.first << "' failed";
  }

  // Test single agent planning
  for (const auto& query : preset_queries_) {
    auto planner = logical_planner::LogicalPlanner::Create(udf_info_).ConsumeValueOrDie();
    auto multi_agent_state =
        logical_planner::testutils::CreateOneAgentOneKelvinPlannerState(schema_);
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query.second);
    auto plan_or_s = planner->Plan(multi_agent_state, query_request);
    EXPECT_OK(plan_or_s) << "Query '" << query.first << "' failed";
  }

  // Test multi agent planning
  for (const auto& query : preset_queries_) {
    auto planner = logical_planner::LogicalPlanner::Create(udf_info_).ConsumeValueOrDie();
    auto multi_agent_state =
        logical_planner::testutils::CreateOneAgentOneKelvinPlannerState(schema_);
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query.second);
    auto plan_or_s = planner->Plan(multi_agent_state, query_request);
    EXPECT_OK(plan_or_s) << "Query '" << query.first << "' failed";
  }
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
