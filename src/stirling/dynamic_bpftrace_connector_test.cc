#include "src/stirling/dynamic_bpftrace_connector.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::pl::stirling::dynamic_tracing::ir::logical::TracepointDeployment_Tracepoint;

TEST(DynamicBPFTraceConnectorTest, Basic) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");

  constexpr char kScript[] = R"(interval:ms:100 {
    printf("%llu %u %llu %s %s\n", nsecs, pid, 0, comm, ntop(0));
  })";

  tracepoint.mutable_bpftrace()->set_program(kScript);

  std::unique_ptr<SourceConnector> connector = DynamicBPFTraceConnector::Create("test", tracepoint);

  const int kTableNum = 0;
  const DataTableSchema& table_schema = connector->TableSchema(kTableNum);

  // Check the inferred table schema.
  {
    const ArrayView<DataElement>& elements = table_schema.elements();

    ASSERT_EQ(elements.size(), 5);

    EXPECT_EQ(elements[0].name(), "time_");
    EXPECT_EQ(elements[0].type(), types::DataType::TIME64NS);

    EXPECT_EQ(elements[1].name(), "tgid_");
    EXPECT_EQ(elements[1].type(), types::DataType::INT64);

    EXPECT_EQ(elements[2].name(), "tgid_start_time_");
    EXPECT_EQ(elements[2].type(), types::DataType::INT64);

    EXPECT_EQ(elements[3].name(), "Column_3");
    EXPECT_EQ(elements[3].type(), types::DataType::STRING);

    EXPECT_EQ(elements[4].name(), "Column_4");
    EXPECT_EQ(elements[4].type(), types::DataType::STRING);
  }

  // Now deploy the spec and check for some data.
  ASSERT_OK(connector->Init());

  // Give some time to collect data.
  sleep(1);

  // Read the data.

  StandaloneContext ctx;
  DataTable data_table(table_schema);
  connector->TransferData(&ctx, kTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

  // Should've gotten something in the records.
  ASSERT_FALSE(tablets.empty());

  // Check that we can gracefully wrap-up.
  ASSERT_OK(connector->Stop());
}

}  // namespace stirling
}  // namespace pl
