#include "src/stirling/dynamic_bpftrace_connector.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::pl::stirling::dynamic_tracing::ir::logical::TracepointDeployment_Tracepoint;

TEST(DynamicBPFTraceConnectorTest, Basic) {
  // Create a BPFTrace program spec
  TracepointDeployment_Tracepoint tracepoint;
  tracepoint.set_table_name("pid_sample_table");
  {
    constexpr char kScript[] = R"(interval:ms:100 {
      printf("%llu %u %s\n", nsecs, pid, comm);
    })";

    auto* bpftrace = tracepoint.mutable_bpftrace();

    bpftrace->set_program(kScript);
    {
      auto* output = bpftrace->add_outputs();
      output->set_name("time");
      output->set_type(types::DataType::INT64);
    }

    {
      auto* output = bpftrace->add_outputs();
      output->set_name("pid");
      output->set_type(types::DataType::INT64);
    }

    {
      auto* output = bpftrace->add_outputs();
      output->set_name("comm");
      output->set_type(types::DataType::STRING);
    }
  }

  // Now deploy the spec and check for some data.
  std::unique_ptr<SourceConnector> connector = DynamicBPFTraceConnector::Create("test", tracepoint);
  ASSERT_OK(connector->Init());

  // Give some time to collect data.
  sleep(1);

  // Read the data.
  const int kTableNum = 0;
  StandaloneContext ctx;
  DataTable data_table(connector->TableSchema(kTableNum));
  connector->TransferData(&ctx, kTableNum, &data_table);
  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();

  // Should've gotten something in the records.
  ASSERT_FALSE(tablets.empty());

  // Check that we can gracefully wrap-up.
  ASSERT_OK(connector->Stop());
}

}  // namespace stirling
}  // namespace pl
