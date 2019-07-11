#pragma once
#include <gtest/gtest.h>

#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

void VerifyMemorySource(IRNode* node) {
  auto mem_node = static_cast<MemorySourceIR*>(node);
  EXPECT_TRUE(mem_node->HasLogicalRepr());
}

void VerifyRange(IRNode* node) {
  auto range_node = static_cast<RangeIR*>(node);
  EXPECT_EQ(range_node->parent()->type(), IRNodeType::kMemorySource);
  EXPECT_EQ(range_node->start_repr()->type(), IRNodeType::kInt);
  EXPECT_EQ(range_node->stop_repr()->type(), IRNodeType::kInt);
  EXPECT_FALSE(range_node->HasLogicalRepr());
}

void VerifyList(IRNode* node) { EXPECT_FALSE(node->HasLogicalRepr()); }

void VerifyString(IRNode* node) { EXPECT_FALSE(node->HasLogicalRepr()); }

void VerifyNodeConnections(IRNode* node) {
  switch (node->type()) {
    case IRNodeType::kMemorySource: {
      VerifyMemorySource(node);
      break;
    }
    case IRNodeType::kRange: {
      VerifyRange(node);
      break;
    }
    case IRNodeType::kList: {
      VerifyList(node);
      break;
    }
    case IRNodeType::kString: {
      VerifyString(node);
      break;
    }
    default: { break; }
  }
}

void VerifyGraphConnections(IR* ig) {
  for (auto& i : ig->dag().TopologicalSort()) {
    VerifyNodeConnections(ig->Get(i));
  }
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
