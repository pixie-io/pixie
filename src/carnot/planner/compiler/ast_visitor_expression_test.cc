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

#include <gtest/gtest.h>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

/**
 * @brief These tests make sure that we can interpret expressions in the ast.
 *
 */

constexpr char kRegInfoProto[] = R"proto(
udas {
  name: "mean"
  update_arg_types: FLOAT64
  finalize_type: FLOAT64
}
scalar_udfs {
  name: "equals"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
)proto";

class ASTExpressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    info_ = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb;
    google::protobuf::TextFormat::MergeFromString(kRegInfoProto, &info_pb);
    PX_CHECK_OK(info_->Init(info_pb));
    compiler_state_ = std::make_unique<CompilerState>(
        std::make_unique<RelationMap>(), /* sensitive_columns */ SensitiveColumnMap{}, info_.get(),
        /* time_now */ time_now_,
        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
        /* redaction_options */ RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});
    graph = std::make_shared<IR>();

    auto ast_visitor_impl = ASTVisitorImpl::Create(graph.get(), &dynamic_trace_,
                                                   compiler_state_.get(), &module_handler_)
                                .ConsumeValueOrDie();
    PX_CHECK_OK(ast_visitor_impl->AddPixieModule());
    ast_visitor = ast_visitor_impl;
  }
  std::shared_ptr<RegistryInfo> info_;
  Parser parser;
  std::shared_ptr<IR> graph;
  std::shared_ptr<ASTVisitor> ast_visitor;

  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
  ModuleHandler module_handler_;
  MutationsIR dynamic_trace_;
};

TEST_F(ASTExpressionTest, String) {
  auto parse_result = parser.Parse("'value'", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());
  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(ExprObject::IsExprObject(obj));
  auto expr = static_cast<ExprObject*>(obj.get())->expr();

  ASSERT_MATCH(expr, String());
  EXPECT_EQ(static_cast<StringIR*>(expr)->str(), "value");
}

TEST_F(ASTExpressionTest, Integer) {
  auto parse_result = parser.Parse("1", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(ExprObject::IsExprObject(obj));
  auto expr = static_cast<ExprObject*>(obj.get())->expr();

  ASSERT_MATCH(expr, Int());
  EXPECT_EQ(static_cast<IntIR*>(expr)->val(), 1);
}

TEST_F(ASTExpressionTest, NegativeInteger) {
  auto parse_result = parser.Parse("-1", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(ExprObject::IsExprObject(obj));
  auto expr = static_cast<ExprObject*>(obj.get())->expr();

  ASSERT_MATCH(expr, Int());
  EXPECT_EQ(static_cast<IntIR*>(expr)->val(), -1);
}

TEST_F(ASTExpressionTest, NegativeFloat) {
  auto parse_result = parser.Parse("-1.0", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(ExprObject::IsExprObject(obj));
  auto expr = static_cast<ExprObject*>(obj.get())->expr();

  ASSERT_MATCH(expr, Float());
  EXPECT_EQ(static_cast<FloatIR*>(expr)->val(), -1);
}

TEST_F(ASTExpressionTest, NegativeLong) {
  auto parse_result = parser.Parse("-2305843009213693952", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(ExprObject::IsExprObject(obj));
  auto expr = static_cast<ExprObject*>(obj.get())->expr();

  ASSERT_MATCH(expr, Int());
  EXPECT_EQ(static_cast<IntIR*>(expr)->val(), -2305843009213693952);
}

TEST_F(ASTExpressionTest, InvertInteger) {
  auto parse_result = parser.Parse("~1", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(ExprObject::IsExprObject(obj));
  auto expr = static_cast<ExprObject*>(obj.get())->expr();

  ASSERT_MATCH(expr, Int());
  EXPECT_EQ(static_cast<IntIR*>(expr)->val(), -2);
}

TEST_F(ASTExpressionTest, PLModule) {
  auto parse_result = parser.Parse("px.mean", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  auto obj = visitor_result.ConsumeValueOrDie();
  EXPECT_EQ(QLObjectType::kFunction, obj->type());
  EXPECT_EQ(std::static_pointer_cast<FuncObject>(obj)->name(), "mean");
}

TEST_F(ASTExpressionTest, PLModuleWrongName) {
  auto parse_result = parser.Parse("px.blah", /* parse_doc_strings */ false);
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_NOT_OK(visitor_result);
  EXPECT_THAT(visitor_result.status(), HasCompilerError("'$0' object has no attribute 'blah'",
                                                        PixieModule::kPixieModuleObjName));
}
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
