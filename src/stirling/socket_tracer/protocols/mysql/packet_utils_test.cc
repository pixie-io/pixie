#include "src/stirling/socket_tracer/protocols/mysql/packet_utils.h"
#include "src/common/testing/testing.h"
#include "src/stirling/socket_tracer/protocols/mysql/test_data.h"
#include "src/stirling/socket_tracer/protocols/mysql/test_utils.h"

namespace pl {
namespace stirling {
namespace protocols {
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

TEST(ProcessBinaryResultsetRow, Basics) {
  ResultsetRow r = testdata::kStmtExecuteResultsetRows[0];
  std::vector<ColDefinition> col_defs = testdata::kStmtExecuteColDefs;
  Packet resultset_row_packet = testutils::GenResultsetRow(0, r);
  EXPECT_OK(ProcessBinaryResultsetRowPacket(resultset_row_packet, col_defs));
}

TEST(ProcessTextResultsetRow, Basics) {
  ResultsetRow r = testdata::kQueryResultsetRows[0];
  Packet resultset_row_packet = testutils::GenResultsetRow(0, r);
  EXPECT_OK(ProcessTextResultsetRowPacket(resultset_row_packet, testdata::kQueryResultset.num_col));
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
