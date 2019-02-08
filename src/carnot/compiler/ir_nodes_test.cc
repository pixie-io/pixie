#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/carnot/compiler/ir_nodes.h"
namespace pl {
namespace carnot {
namespace compiler {

TEST(IRGraph, FromRangeIRGraph) {
  /**
   * Same output as compiling
   *
   * `From(table="tableName", select=["testCol"]).Range("-2m")`
   */

  IR ig;
  auto src = ig.MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = ig.MakeNode<RangeIR>().ValueOrDie();
  auto rng_str = ig.MakeNode<StringIR>().ValueOrDie();
  auto table_str = ig.MakeNode<StringIR>().ValueOrDie();
  auto select_col = ig.MakeNode<StringIR>().ValueOrDie();
  auto select_list = ig.MakeNode<ListIR>().ValueOrDie();
  EXPECT_TRUE(rng_str->Init("-2m").ok());
  EXPECT_TRUE(table_str->Init("tableName").ok());
  EXPECT_TRUE(select_col->Init("testCol").ok());
  EXPECT_TRUE(select_list->AddListItem(select_col).ok());
  EXPECT_TRUE(src->Init(table_str, select_list).ok());
  EXPECT_TRUE(range->Init(src, rng_str).ok());
  std::cout << ig.DebugString() << std::endl;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
