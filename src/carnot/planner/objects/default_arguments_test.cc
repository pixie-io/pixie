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
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/metadata_object.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::px::table_store::schema::Relation;
using ::testing::ElementsAre;

constexpr char kRegInfoProto[] = R"proto(
scalar_udfs {
  name: "px.equals"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
)proto";
class DefaultArgumentsTest : public OperatorTests {
 protected:
  std::unique_ptr<planner::RegistryInfo> SetUpRegistryInfo() {
    udfspb::UDFInfo udf_proto;
    google::protobuf::TextFormat::MergeFromString(kRegInfoProto, &udf_proto);

    auto info = std::make_unique<planner::RegistryInfo>();
    PX_CHECK_OK(info->Init(udf_proto));
    return info;
  }

  std::unique_ptr<RelationMap> SetUpRelMap() {
    auto rel_map = std::make_unique<RelationMap>();
    rel_map->emplace("sequences", Relation(
                                      {
                                          types::TIME64NS,
                                          types::FLOAT64,
                                          types::FLOAT64,
                                      },
                                      {"time_", "xmod10", "PIx"}));
    return rel_map;
  }

  void SetUp() override {
    OperatorTests::SetUp();
    info_ = SetUpRegistryInfo();
    compiler_state_ = std::make_unique<CompilerState>(
        SetUpRelMap(), /* sensitive_columns */ SensitiveColumnMap{}, info_.get(),
        /* time_now */ time_now_,
        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",

        /* redaction_options */ RedactionOptions{}, nullptr,
        std::unique_ptr<PluginConfig>(new PluginConfig{0, 0}), planner::DebugInfo{});

    ast_visitor_ = ASTVisitorImpl::Create(graph.get(), &dynamic_trace_, compiler_state_.get(),
                                          &module_handler_)
                       .ConsumeValueOrDie();
    module_ = PixieModule::Create(graph.get(), compiler_state_.get(), ast_visitor_.get())
                  .ConsumeValueOrDie();
  }

  void TestFunctionDefaults(std::shared_ptr<FuncObject> func_object, std::string_view function_str,
                            bool should_fail = false) {
    SCOPED_TRACE(function_str);
    for (const auto& [default_arg_name, default_arg_value] : func_object->defaults()) {
      auto expr_or_s =
          ast_visitor_->ParseAndProcessSingleExpression(default_arg_value, /*import px*/ true);
      if (should_fail) {
        EXPECT_NOT_OK(expr_or_s);
      } else {
        EXPECT_OK(expr_or_s) << absl::Substitute(
            "for func '$0' the default value for argument '$1' failed to be parsed - '$2'",
            function_str, default_arg_name, default_arg_value);
      }
    }
  }

  void VerifyObjectDefaults(QLObjectPtr object, std::string_view name, bool should_fail = false) {
    SCOPED_TRACE(object->type_descriptor().name());
    for (const auto& [method_name, method_func] : object->methods()) {
      std::string path_str = absl::Substitute("$0->$1", name, method_name);
      TestFunctionDefaults(method_func, path_str, should_fail);
    }
    for (const auto& attr_name : object->AllAttributes()) {
      std::string path_str = absl::Substitute("$0->$1", name, attr_name);
      auto attr_object = object->GetAttribute(ast, attr_name).ConsumeValueOrDie();
      VerifyObjectDefaults(attr_object, path_str, should_fail);
    }
  }

  std::shared_ptr<ASTVisitorImpl> ast_visitor_;
  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
  std::unique_ptr<RegistryInfo> info_;
  std::shared_ptr<PixieModule> module_;
  ModuleHandler module_handler_;
  MutationsIR dynamic_trace_;
};

// This test is the unit to make sure we can get all of the defaults of any method for any object
// we choose to test.
TEST_F(DefaultArgumentsTest, PLModule) {
  auto module_or_s = PixieModule::Create(graph.get(), compiler_state_.get(), ast_visitor_.get());
  ASSERT_OK(module_or_s);
  QLObjectPtr module = module_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("pl_module");
    VerifyObjectDefaults(module, "pl_module");
  }
}

TEST_F(DefaultArgumentsTest, Dataframe) {
  auto dataframe_or_s =
      Dataframe::Create(compiler_state_.get(), MakeMemSource(), ast_visitor_.get());
  ASSERT_OK(dataframe_or_s);
  QLObjectPtr obj = dataframe_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("Dataframe");
    VerifyObjectDefaults(obj, "Dataframe");
  }
}

TEST_F(DefaultArgumentsTest, Metadata) {
  auto metadata_or_s = MetadataObject::Create(MakeMemSource(), ast_visitor_.get());
  ASSERT_OK(metadata_or_s);
  QLObjectPtr metadata = metadata_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("Metadata");
    VerifyObjectDefaults(metadata, "Metadata");
  }
}

TEST_F(DefaultArgumentsTest, Expr) {
  auto expr_or_s = ExprObject::Create(MakeInt(10), ast_visitor_.get());
  ASSERT_OK(expr_or_s);
  QLObjectPtr expr = expr_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("Expr");
    VerifyObjectDefaults(expr, "Expr");
  }
}

/**
 * @brief This test has a syntax error inthe default argument of the only method. This makes sure
 * that we can fail upon finding this issue.
 *
 */
class TestQLObject : public QLObject {
 public:
  static constexpr TypeDescriptor TestQLObjectType = {
      /* name */ "TestQLObject",
      /* type */ QLObjectType::kMisc,
  };

  TestQLObject(bool has_attr, ASTVisitor* visitor) : QLObject(TestQLObjectType, visitor) {
    std::shared_ptr<FuncObject> func_obj =
        FuncObject::Create("default_func", {"arg"}, {{"arg", "d("}},
                           /* has_variable_len_args */ false,
                           /* has_variable_len_kwargs */ false,
                           std::bind(&TestQLObject::SimpleFunc, this, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3),
                           visitor)
            .ConsumeValueOrDie();
    AddMethod("default_func", func_obj);
    if (has_attr) {
      PX_CHECK_OK(AssignAttribute(kSpecialAttr, std::make_shared<NoneObject>(visitor)));
    }
  }

  StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr&, const ParsedArgs&, ASTVisitor* visitor) {
    auto out_obj = std::make_shared<TestQLObject>(false, visitor);
    return StatusOr<QLObjectPtr>(out_obj);
  }

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr&,
                                         std::string_view name) const override {
    DCHECK(HasNonMethodAttribute(name));
    // Set to false so our test doesn't recurse forever.
    auto out_obj = std::make_shared<TestQLObject>(false, ast_visitor());
    out_obj->SetName(std::string(name));
    return StatusOr<QLObjectPtr>(out_obj);
  }

  void SetName(const std::string& name) { internal_name_ = name; }
  const std::string& internal_name() const { return internal_name_; }

  inline static constexpr char kSpecialAttr[] = "foobar";

 private:
  std::string internal_name_;
};

TEST_F(DefaultArgumentsTest, TestQLObject) {
  QLObjectPtr qlobject = std::make_shared<TestQLObject>(true, ast_visitor_.get());
  {
    SCOPED_TRACE("TestQLObject");
    VerifyObjectDefaults(qlobject, "TestQLObject", /* should_fail */ true);
  }
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
