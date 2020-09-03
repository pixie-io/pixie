#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/type_resolver_util.h>
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
#include "src/shared/vispb/vis.pb.h"
#include "src/stirling/stirling.h"
#include "src/table_store/schema/schema.h"

namespace pl {
namespace carnot {
namespace planner {

using ::google::protobuf::DescriptorPool;
using ::google::protobuf::util::JsonToBinaryString;
using ::google::protobuf::util::NewTypeResolverForDescriptorPool;
using ::google::protobuf::util::TypeResolver;
using ::pl::table_store::schema::Relation;
using ::pl::testing::proto::EqualsProto;
using ::pl::vispb::Vis;
using planpb::testutils::CompareLogicalPlans;
using ::testing::_;
using ::testing::ContainsRegex;

struct LiveView {
  std::string pxl_script;
  std::string vis_spec;
};

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

    compiler_state_ =
        std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now, "result_addr");
    compiler_ = compiler::Compiler();

    EXPECT_OK(table_store::schema::Schema::ToProto(&schema_, absl_rel_map));
    ParsePresetQueries();
  }

  void AddOrCreateVisSpec(const std::filesystem::path& vis_spec_path) {
    std::string parent_path = vis_spec_path.parent_path().string();
    if (!preset_scripts_.contains(parent_path)) {
      preset_scripts_[parent_path] = {};
    }
    preset_scripts_[parent_path].vis_spec = vis_spec_path.string();
  }

  void AddOrCreatePxlScript(const std::filesystem::path& pxl_script_path) {
    std::string parent_path = pxl_script_path.parent_path().string();
    if (!preset_scripts_.contains(parent_path)) {
      preset_scripts_[parent_path] = {};
    }
    PL_ASSIGN_OR_EXIT(preset_scripts_[parent_path].pxl_script,
                      ReadFileToString(pxl_script_path.string()));
  }

  void ParsePresetQueries() {
    absl::flat_hash_set<std::string> ignore_directories;
    // 1st pass: find directories to ignore.
    for (const auto& entry : std::filesystem::recursive_directory_iterator(scripts_dir_)) {
      std::string strpath = entry.path().string();
      // Ignore the live views for now, no guaranteee that the values will be correct.
      if (absl::EndsWith(strpath, "vis.json")) {
        AddOrCreateVisSpec(entry.path());
      } else if (absl::EndsWith(strpath, ".pxl")) {
        AddOrCreatePxlScript(entry.path());
      }
    }
    ASSERT_GT(preset_scripts_.size(), 0);
  }

  compiler::FuncToExecute ParseFunc(
      const absl::flat_hash_map<std::string, std::string>& variable_map,
      const pl::vispb::Widget_Func& widget_func, const std::string& table_name) {
    compiler::FuncToExecute exec_func;
    exec_func.set_output_table_prefix(table_name);
    exec_func.set_func_name(widget_func.name());
    for (const auto& arg_val : widget_func.args()) {
      auto exec_func_arg_value = exec_func.add_arg_values();
      exec_func_arg_value->set_name(arg_val.name());
      switch (arg_val.input_case()) {
        case vispb::Widget_Func_FuncArg::kValue: {
          exec_func_arg_value->set_value(arg_val.value());
          break;
        }
        case vispb::Widget_Func_FuncArg::kVariable: {
          // Pass up that parsing failed. Not a Check because we should surface multiple errors if
          // they exist.
          EXPECT_TRUE(variable_map.contains(arg_val.name()))
              << absl::Substitute("Variable $0 not found", arg_val.name());

          if (!variable_map.contains(arg_val.name())) {
            break;
          }
          exec_func_arg_value->set_value(variable_map.find(arg_val.name())->second);
          break;
        }
        default: {
          EXPECT_TRUE(false) << absl::Substitute("'$0' arg type not handled",
                                                 magic_enum::enum_name(arg_val.input_case()));
        }
      }
    }
    return exec_func;
  }

  compiler::ExecFuncs GetExecFuncs(const LiveView& lv) {
    // If the vis spec is empty, the live view only returns raw tables.
    if (lv.vis_spec == "") {
      return {};
    }

    // Load vis spec as a protobuf.
    // First load as a JSON string.
    auto vis_json_or_s = ReadFileToString(lv.vis_spec);
    EXPECT_OK(vis_json_or_s);
    if (!vis_json_or_s.ok()) {
      return {};
    }
    auto vis_json = vis_json_or_s.ConsumeValueOrDie();
    std::string output;
    // Then resolve JSON string to a protobuf serialized string.
    auto resolver = std::unique_ptr<TypeResolver>(
        NewTypeResolverForDescriptorPool("pixielabs.ai", DescriptorPool::generated_pool()));
    auto status =
        JsonToBinaryString(resolver.get(), "pixielabs.ai/pl.vispb.Vis", vis_json, &output);
    EXPECT_TRUE(status.ok()) << status.error_message();
    if (!status.ok()) {
      return {};
    }
    // Finally parse protobuf serialization to struct.
    Vis vs;
    bool parse_from_string_succesful = vs.ParseFromString(output);
    EXPECT_TRUE(parse_from_string_succesful);
    if (!parse_from_string_succesful) {
      return {};
    }

    // Parse global variables.
    absl::flat_hash_map<std::string, std::string> variable_map;
    for (const auto& var : vs.variables()) {
      variable_map[var.name()] = var.default_value();
    }

    compiler::ExecFuncs exec_funcs;
    // Parse global functions.
    absl::flat_hash_set<std::string> global_func_output_names;
    for (const auto& gf : vs.global_funcs()) {
      global_func_output_names.insert(gf.output_name());
      exec_funcs.push_back(ParseFunc(variable_map, gf.func(), gf.output_name()));
    }

    // Parse functions out of widgets. Also make sure any referenced GlobalFuncOutputNames are
    // actually defined.
    for (const auto& widget : vs.widgets()) {
      switch (widget.func_or_ref_case()) {
        case (vispb::Widget::kFunc): {
          std::string widget_name = widget.name();
          exec_funcs.push_back(ParseFunc(variable_map, widget.func(), widget_name));
          break;
        }
        case (vispb::Widget::kGlobalFuncOutputName): {
          // Verify the global func referenced is defined.
          EXPECT_TRUE(global_func_output_names.contains(widget.global_func_output_name()))
              << absl::Substitute("'$0' global func not found among defined funcs: '$1'",
                                  widget.global_func_output_name(),
                                  absl::StrJoin(global_func_output_names, ","));
          break;
        }
        default: {
          EXPECT_TRUE(false) << absl::Substitute(
              "'$0' reference type not supported",
              magic_enum::enum_name(vispb::Widget::kGlobalFuncOutputName));
        }
      }
    }

    return exec_funcs;
  }

  void SetExecFuncs(const LiveView& lv, plannerpb::QueryRequest* query_request) {
    for (const auto& func : GetExecFuncs(lv)) {
      (*query_request->add_exec_funcs()) = func;
    }
  }

  absl::flat_hash_map<std::string, LiveView> preset_scripts_;
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  compiler::Compiler compiler_;
  table_store::schemapb::Schema schema_;
  Relation cgroups_relation_;
  const std::string scripts_dir_ = "src/pxl_scripts/px";
  udfspb::UDFInfo udf_info_;
};

// TODO(nserrino): PP-2188 Update this test to download the public scripts from the github repo.
TEST_F(PresetQueriesTest, DISABLED_PresetQueries) {
  // Test single-node compiler (no distributed planner).
  for (const auto& [path, script] : preset_scripts_) {
    SCOPED_TRACE(absl::Substitute("Single agent for '$0'", path));
    auto exec_funcs = GetExecFuncs(script);
    auto plan_or_s = compiler_.Compile(script.pxl_script, compiler_state_.get(), exec_funcs);
    EXPECT_OK(plan_or_s) << "Query failed";
  }

  // Test single agent planning.
  for (const auto& [path, script] : preset_scripts_) {
    SCOPED_TRACE(absl::Substitute("Single agent for '$0'", path));
    auto planner = LogicalPlanner::Create(udf_info_).ConsumeValueOrDie();
    auto single_pem_state = testutils::CreateOnePEMOneKelvinPlannerState(schema_);
    single_pem_state.mutable_plan_options()->set_max_output_rows_per_table(10000);
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(script.pxl_script);
    SetExecFuncs(script, &query_request);
    auto plan_or_s = planner->Plan(single_pem_state, query_request);
    EXPECT_OK(plan_or_s) << "Query failed";
    auto plan = plan_or_s.ConsumeValueOrDie();
    EXPECT_OK(plan->ToProto()) << "Query failed to compile to proto";
  }

  // Test multi agent planning.
  for (const auto& [path, script] : preset_scripts_) {
    SCOPED_TRACE(absl::Substitute("Single agent for '$0'", path));
    auto planner = LogicalPlanner::Create(udf_info_).ConsumeValueOrDie();
    auto multi_pem_state = testutils::CreateTwoPEMsOneKelvinPlannerState(schema_);
    multi_pem_state.mutable_plan_options()->set_max_output_rows_per_table(10000);
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(script.pxl_script);
    SetExecFuncs(script, &query_request);
    auto plan_or_s = planner->Plan(multi_pem_state, query_request);
    EXPECT_OK(plan_or_s) << "Query failed";
    auto plan = plan_or_s.ConsumeValueOrDie();
    EXPECT_OK(plan->ToProto()) << "Query failed to compile to proto";
  }
}

}  // namespace planner
}  // namespace carnot
}  // namespace pl
