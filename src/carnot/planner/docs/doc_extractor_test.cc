#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

#include "src/carnot/planner/docs/doc_extractor.h"

namespace pl {
namespace carnot {
namespace planner {
namespace docs {

class DocExtractorTest : public ::testing::Test {};
TEST_F(DocExtractorTest, can_get_module_docs) {
  IR ir;
  compiler::MutationsIR dynamic_trace;
  RegistryInfo registry_info;
  CompilerState compiler_state(std::make_unique<RelationMap>(), &registry_info, 10, "result");
  compiler::ModuleHandler module_handler;

  ASSERT_OK_AND_ASSIGN(
      auto ast_visitor,
      compiler::ASTVisitorImpl::Create(&ir, &dynamic_trace, &compiler_state, &module_handler));

  DocExtractor extractor;
  for (const auto& [modname, module] : module_handler) {
    auto mod_doc = extractor.ExtractDoc(module);
    docspb::DocstringNode pb;
    ASSERT_OK(mod_doc.ToProto(&pb));
    LOG(INFO) << absl::Substitute("\n\"$0\": $1", modname, pb.DebugString());
  }
}

TEST_F(DocExtractorTest, test_object_strings) {
  IR ir;
  compiler::MutationsIR dynamic_trace;
  RegistryInfo registry_info;
  CompilerState compiler_state(std::make_unique<RelationMap>(), &registry_info, 10, "result");
  compiler::ModuleHandler module_handler;

  ASSERT_OK_AND_ASSIGN(
      auto ast_visitor,
      compiler::ASTVisitorImpl::Create(&ir, &dynamic_trace, &compiler_state, &module_handler));

  DocExtractor extractor;
  auto module_name = compiler::TraceModule::kTraceModuleObjName;
  ASSERT_TRUE(module_handler.contains(module_name));
  auto traceobj = module_handler[module_name];
  auto mod_doc = extractor.ExtractDoc(traceobj);
  docspb::DocstringNode pb;
  ASSERT_OK(mod_doc.ToProto(&pb));

  EXPECT_EQ(pb.name(), module_name);
  // pxtrace module has no docstring so should be empty.
  EXPECT_EQ(pb.docstring(), "");
  docspb::DocstringNode tracepoint_doc_node;
  std::string tracepoint_name = compiler::TraceModule::kUpsertTraceID;
  for (const auto& c : pb.children()) {
    if (c.name() == tracepoint_name) {
      tracepoint_doc_node = c;
      break;
    }
  }
  EXPECT_EQ(tracepoint_doc_node.name(), tracepoint_name);
  EXPECT_THAT(tracepoint_doc_node.docstring(), ::testing::ContainsRegex("Deploys a tracepoint"));
}

}  // namespace docs
}  // namespace planner
}  // namespace carnot
}  // namespace pl
