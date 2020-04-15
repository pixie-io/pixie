#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/plannerpb/func_args.pb.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"
#include "src/shared/schema/utils.h"
#include "src/stirling/stirling.h"
#include "src/table_store/schema/schema.h"

namespace pl {
namespace carnot {
namespace planner {

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
    auto stirling = stirling::Stirling::Create(stirling::CreateSourceRegistry());
    stirling::stirlingpb::Publish publish_pb;
    stirling->GetPublishProto(&publish_pb);
    auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);
    auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);

    for (const auto& rel_info : relation_info_vec) {
      rel_map->emplace(rel_info.name, rel_info.relation);
      absl_rel_map[rel_info.name] = rel_info.relation;
    }

    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);
    compiler_ = compiler::Compiler();

    EXPECT_OK(table_store::schema::Schema::ToProto(&schema_, absl_rel_map));
    ParsePresetQueries();
  }

  void ParsePresetQueries() {
    absl::flat_hash_set<std::string> ignore_directories;
    // 1st pass: find directories to ignore.
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scripts_dir_)) {
      std::string strpath = entry.path().string();
      // Ignore the live views for now, no guaranteee that the values will be correct.
      if (absl::EndsWith(strpath, ".json")) {
        std::filesystem::path p = strpath;
        ignore_directories.insert(p.parent_path().string());
      }
    }

    // 2nd pass: add queries to test
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scripts_dir_)) {
      std::string strpath = entry.path().string();
      std::filesystem::path p = strpath;
      if (!absl::EndsWith(strpath, ".pxl") ||
          ignore_directories.contains(p.parent_path().string())) {
        // Ignore the live views for now, no guaranteee that the values will be correct.
        continue;
      }
      PL_ASSIGN_OR_EXIT(preset_queries_[strpath], ReadFileToString(entry.path()));
    }
    ASSERT_GT(preset_queries_.size(), 0);
  }

  // Using map so that test order is deterministic.
  std::map<std::string, std::string> preset_queries_;
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  compiler::Compiler compiler_;
  table_store::schemapb::Schema schema_;
  Relation cgroups_relation_;
  const std::string scripts_dir_ = "src/pxl_scripts/px";
  udfspb::UDFInfo udf_info_;
};

TEST_F(PresetQueriesTest, PresetQueries) {
  // Test compilation
  for (const auto& query : preset_queries_) {
    auto plan = compiler_.Compile(query.second, compiler_state_.get());
    ASSERT_OK(plan) << "Query '" << query.first << "' failed";
  }

  // Test single agent planning
  for (const auto& query : preset_queries_) {
    auto planner = LogicalPlanner::Create(udf_info_).ConsumeValueOrDie();
    auto single_agent_state = testutils::CreateOneAgentOneKelvinPlannerState(schema_);
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query.second);
    auto plan_or_s = planner->Plan(single_agent_state, query_request);
    EXPECT_OK(plan_or_s) << "Query '" << query.first << "' failed";
    auto plan = plan_or_s.ConsumeValueOrDie();
    EXPECT_OK(plan->ToProto()) << "Query '" << query.first << "' failed to compile to proto";
  }

  // Test multi agent planning
  for (const auto& query : preset_queries_) {
    auto planner = LogicalPlanner::Create(udf_info_).ConsumeValueOrDie();
    auto multi_agent_state = testutils::CreateTwoAgentsOneKelvinPlannerState(schema_);
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query.second);
    auto plan_or_s = planner->Plan(multi_agent_state, query_request);
    EXPECT_OK(plan_or_s) << "Query '" << query.first << "' failed";
    auto plan = plan_or_s.ConsumeValueOrDie();
    EXPECT_OK(plan->ToProto()) << "Query '" << query.first << "' failed to compile to proto";
  }
}

}  // namespace planner
}  // namespace carnot
}  // namespace pl
