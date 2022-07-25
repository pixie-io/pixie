/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

#include "src/carnot/planner/docs/doc_extractor.h"

namespace px {
namespace carnot {
namespace planner {
namespace docs {

class DocExtractorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    compiler_state = std::make_unique<CompilerState>(
        std::make_unique<RelationMap>(), SensitiveColumnMap{}, &registry_info, /* time_now */ 0,
        /* max_output_rows_per_table */ 0,
        /* result_addr */ "", /* ssl_targetname_override */ "", RedactionOptions{}, nullptr,
        nullptr, planner::DebugInfo{});
  }
  std::unique_ptr<CompilerState> compiler_state;
  IR ir;
  compiler::MutationsIR dynamic_trace;
  RegistryInfo registry_info;
  compiler::ModuleHandler module_handler;
};
TEST_F(DocExtractorTest, can_get_module_docs) {
  ASSERT_OK_AND_ASSIGN(
      auto ast_visitor,
      compiler::ASTVisitorImpl::Create(&ir, &dynamic_trace, compiler_state.get(), &module_handler));

  DocExtractor extractor;
  for (const auto& [modname, module] : module_handler) {
    auto mod_doc = extractor.ExtractDoc(module);
    docspb::DocstringNode pb;
    ASSERT_OK(mod_doc.ToProto(&pb));
    LOG(INFO) << absl::Substitute("\n\"$0\": $1", modname, pb.DebugString());
  }
}

TEST_F(DocExtractorTest, test_object_strings) {
  ASSERT_OK_AND_ASSIGN(
      auto ast_visitor,
      compiler::ASTVisitorImpl::Create(&ir, &dynamic_trace, compiler_state.get(), &module_handler));

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
}  // namespace px
