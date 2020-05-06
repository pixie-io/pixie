#include "src/stirling/mysql/packet_utils.h"
#include "src/common/testing/testing.h"
#include "src/stirling/mysql/test_data.h"
#include "src/stirling/mysql/test_utils.h"

namespace pl {
namespace stirling {
namespace mysql {

TEST(ProcessColDefinition, Basics) {
  ColDefinition expected_col_def = testdata::kStmtPrepareColDefs[0];
  Packet col_def_packet = testutils::GenColDefinition(0, expected_col_def);
  auto s = ProcessColumnDefPacket(col_def_packet);
  EXPECT_OK(s);
  ColDefinition col_def = s.ValueOrDie();
  EXPECT_EQ(col_def.schema, expected_col_def.schema);
  EXPECT_EQ(col_def.table, expected_col_def.table);
  EXPECT_EQ(col_def.org_table, expected_col_def.org_table);
  EXPECT_EQ(col_def.name, expected_col_def.name);
  EXPECT_EQ(col_def.org_name, expected_col_def.org_name);
  EXPECT_EQ(col_def.next_length, expected_col_def.next_length);
  EXPECT_EQ(col_def.character_set, expected_col_def.character_set);
  EXPECT_EQ(col_def.column_length, expected_col_def.column_length);
  EXPECT_EQ(col_def.column_type, expected_col_def.column_type);
  EXPECT_EQ(col_def.flags, expected_col_def.flags);
  EXPECT_EQ(col_def.decimals, expected_col_def.decimals);
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
