#include <gtest/gtest.h>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/parser/parser.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
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
  name: "pl.mean"
  update_arg_types: FLOAT64
  finalize_type: FLOAT64
}
scalar_udfs {
  name: "pl.equals"
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
    PL_CHECK_OK(info_->Init(info_pb));
    compiler_state_ =
        std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info_.get(), time_now_);
    graph = std::make_shared<IR>();
    ast_visitor = ASTVisitorImpl::Create(graph.get(), compiler_state_.get()).ConsumeValueOrDie();
  }
  std::shared_ptr<RegistryInfo> info_;
  Parser parser;
  std::shared_ptr<IR> graph;
  std::shared_ptr<ASTVisitor> ast_visitor;

  std::shared_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
};

TEST_F(ASTExpressionTest, String) {
  auto parse_result = parser.Parse("'value'");
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());
  ASSERT_OK(visitor_result);

  IRNode* node = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(Match(node, String()));
  EXPECT_EQ(static_cast<StringIR*>(node)->str(), "value");
}

TEST_F(ASTExpressionTest, Integer) {
  auto parse_result = parser.Parse("1");
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  IRNode* node = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(Match(node, Int()));
  EXPECT_EQ(static_cast<IntIR*>(node)->val(), 1);
}

TEST_F(ASTExpressionTest, PLModule) {
  auto parse_result = parser.Parse("pl.mean");
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_OK(visitor_result);

  IRNode* node = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(Match(node, Func()));
  EXPECT_EQ(static_cast<FuncIR*>(node)->carnot_op_name(), "mean");
}

TEST_F(ASTExpressionTest, PLModuleWrongName) {
  auto parse_result = parser.Parse("pl.blah");
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  ASSERT_NOT_OK(visitor_result);
  EXPECT_THAT(visitor_result.status(), HasCompilerError("'pl' object has no attribute 'blah'"));
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
