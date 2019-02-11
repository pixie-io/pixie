#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {
/**
 * Creates IR Graph that is the following query compiled
 *
 * `From(table="tableName", select=["testCol"]).Range("-2m")`
 */

TEST(IRTest, check_connection) {
  auto ig = std::make_shared<IR>();
  auto src = ig->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = ig->MakeNode<RangeIR>().ValueOrDie();
  auto rng_str = ig->MakeNode<StringIR>().ValueOrDie();
  auto table_str = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_col = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_list = ig->MakeNode<ListIR>().ValueOrDie();
  EXPECT_TRUE(rng_str->Init("-2m").ok());
  EXPECT_TRUE(table_str->Init("tableName").ok());
  EXPECT_TRUE(select_col->Init("testCol").ok());
  EXPECT_TRUE(select_list->AddListItem(select_col).ok());
  EXPECT_TRUE(src->Init(table_str, select_list).ok());
  EXPECT_TRUE(range->Init(src, rng_str).ok());
  EXPECT_EQ(range->parent(), src);
  EXPECT_EQ(range->time_repr(), rng_str);
  EXPECT_EQ(src->table_node(), table_str);
  EXPECT_EQ(src->select(), select_list);
  EXPECT_EQ(select_list->children()[0], select_col);
  EXPECT_EQ(select_col->str(), "testCol");
  VerifyGraphConnections(ig.get());
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
