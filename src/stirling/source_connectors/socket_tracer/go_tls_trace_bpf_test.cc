#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/base/test_utils.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/testing/protocol_checkers.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"
#include "src/stirling/testing/common.h"

namespace pl {
namespace stirling {

namespace http = protocols::http;

// Automatically converts ToString() to stream operator for gtest.
using ::pl::operator<<;

using ::pl::stirling::testing::AccessRecordBatch;
using ::pl::stirling::testing::EqHTTPRecord;
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::ToRecordVector;
using ::pl::testing::BazelBinTestFilePath;

using ::testing::UnorderedElementsAre;

//-----------------------------------------------------------------------------
// Test Stimulus: Server and Client
//-----------------------------------------------------------------------------

class GoTLSServerContainer : public ContainerRunner {
 public:
  GoTLSServerContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/server/server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "https_server";
  static constexpr std::string_view kReadyMessage = "Starting HTTPS service";
};

class GoTLSClientContainer : public ContainerRunner {
 public:
  GoTLSClientContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/demo_apps/go_https/client/client_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "https_client";
  static constexpr std::string_view kReadyMessage = R"({"status":"ok"})";
};

//-----------------------------------------------------------------------------
// Test Class and Test Cases
//-----------------------------------------------------------------------------

class GoTLSTraceTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  GoTLSTraceTest() {
    // Run the server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    PL_CHECK_OK(server_.Run(60, {}));
  }

  GoTLSServerContainer server_;
  GoTLSClientContainer client_;
};

//-----------------------------------------------------------------------------
// Result Checking: Helper Functions and Matchers
//-----------------------------------------------------------------------------

TEST_F(GoTLSTraceTest, Basic) {
  StartTransferDataThread(SocketTraceConnector::kHTTPTableNum, kHTTPTable);

  // Run the client in the network of the server, so they can connect to each other.
  PL_CHECK_OK(
      client_.Run(10, {absl::Substitute("--network=container:$0", server_.container_name())}));
  client_.Wait();

  // We do not expect this sleep to be required, but it appears to be necessary for Jenkins.
  // TODO(oazizi): Figure out why.
  sleep(3);

  // Grab the data from Stirling.
  std::vector<TaggedRecordBatch> tablets = StopTransferDataThread();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  {
    const std::vector<size_t> target_record_indices =
        FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, server_.process_pid());

    std::vector<http::Record> records = ToRecordVector(record_batch, target_record_indices);

    // TODO(oazizi): Add headers checking too.
    http::Record expected_record = {};
    expected_record.req.req_path = "/";
    expected_record.req.req_method = "GET";
    expected_record.req.body = R"()";
    expected_record.resp.resp_status = 200;
    expected_record.resp.resp_message = "OK";
    expected_record.resp.body = R"({"status":"ok"})";

    EXPECT_THAT(records, Contains(EqHTTPRecord(expected_record)));
  }
}

}  // namespace stirling
}  // namespace pl
