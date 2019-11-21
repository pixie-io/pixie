#pragma once

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

class QLObjectTest : public OperatorTests {
 protected:
  void SetUp() override {
    OperatorTests::SetUp();

    auto info = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb;
    PL_CHECK_OK(info->Init(info_pb));
    auto compiler_state =
        std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info.get(), 0);
    // Graph is set in OperatorTests.
    ast_visitor = ASTVisitorImpl::Create(graph.get(), compiler_state.get()).ConsumeValueOrDie();
  }
  std::shared_ptr<ASTVisitor> ast_visitor;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
