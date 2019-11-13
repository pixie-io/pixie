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

class ASTExpressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto info = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb;
    PL_CHECK_OK(info->Init(info_pb));
    auto compiler_state =
        std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info.get(), 0);
    graph = std::make_shared<IR>();
    ast_visitor = std::make_shared<ASTVisitorImpl>(graph.get(), compiler_state.get());
  }
  Parser parser;
  std::shared_ptr<IR> graph;
  std::shared_ptr<ASTVisitor> ast_visitor;
};

TEST_F(ASTExpressionTest, String) {
  auto parse_result = parser.Parse("'value'");
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  IRNode* node = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(Match(node, String()));
  EXPECT_EQ(static_cast<StringIR*>(node)->str(), "value");
}

TEST_F(ASTExpressionTest, Integer) {
  auto parse_result = parser.Parse("1");
  auto visitor_result =
      ast_visitor->ProcessSingleExpressionModule(parse_result.ConsumeValueOrDie());

  IRNode* node = visitor_result.ConsumeValueOrDie();
  ASSERT_TRUE(Match(node, Int()));
  EXPECT_EQ(static_cast<IntIR*>(node)->val(), 1);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
