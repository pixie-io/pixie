#pragma once
#include <gtest/gtest.h>

#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

void VerifyMemorySource(IRNode* node) {
  auto mem_node = static_cast<MemorySourceIR*>(node);
  EXPECT_EQ(mem_node->table_node()->type(), IRNodeType::StringType);
  EXPECT_EQ(mem_node->select()->type(), IRNodeType::ListType);
  EXPECT_TRUE(mem_node->HasLogicalRepr());
  // TODO(oazizi or zasgar) - what do you think about doing checks using dependencies of?
  // I'm not sure what we can accomplish, was wondering if you thoguth it might bet better than the
  // tests above. They likely overlap in terms of coverage.
  //
  // node->graph_ptr()->dag().DependenciesOf(node->id());
}

void VerifyRange(IRNode* node) {
  auto range_node = static_cast<RangeIR*>(node);
  EXPECT_EQ(range_node->parent()->type(), IRNodeType::MemorySourceType);
  EXPECT_EQ(range_node->time_repr()->type(), IRNodeType::StringType);
  EXPECT_FALSE(range_node->HasLogicalRepr());
}

void VerifyList(IRNode* node) { EXPECT_FALSE(node->HasLogicalRepr()); }

void VerifyString(IRNode* node) { EXPECT_FALSE(node->HasLogicalRepr()); }

void VerifyNodeConnections(IRNode* node) {
  switch (node->type()) {
    case IRNodeType::MemorySourceType: {
      VerifyMemorySource(node);
      break;
    }
    case IRNodeType::RangeType: {
      VerifyRange(node);
      break;
    }
    case IRNodeType::ListType: {
      VerifyList(node);
      break;
    }
    case IRNodeType::StringType: {
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
