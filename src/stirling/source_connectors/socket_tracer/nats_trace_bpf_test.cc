/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_connector.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nats_client_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/container_images/nats_server_container.h"
#include "src/stirling/source_connectors/socket_tracer/testing/socket_trace_bpf_test_fixture.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::FindRecordIdxMatchesPID;
using ::px::testing::BazelRunfilePath;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;
// Automatically converts ToString() to stream operator for gtest.
using ::px::operator<<;

class NATSTraceBPFTest : public testing::SocketTraceBPFTestFixture</* TClientSideTracing */ false>,
                         public ::testing::WithParamInterface<bool> {
 protected:
  NATSTraceBPFTest() {
    FLAGS_stirling_enable_nats_tracing = true;
    std::vector<std::string> args;
    if (GetParam()) {
      // https://docs.nats.io/nats-server/configuration/securing_nats/tls
      // shows similar configuration implemented in config file, but did
      // not work, for some reason. So we use command line flags.
      args = {"--tls", "--tlscert=/etc/ssl/server.crt", "--tlskey=/etc/ssl/server.key",
              "--tlsverify=false"};
    }
    PX_CHECK_OK(server_container_.Run(std::chrono::seconds{150}, /*options*/ {}, args));
  }

  ::px::stirling::testing::NATSServerContainer server_container_;
  ::px::stirling::testing::NATSClientContainer client_container_;
};

struct NATSTraceRecord {
  int64_t ts_ns;
  std::string cmd;
  std::string options;
  std::string resp;

  std::string ToString() const {
    return absl::Substitute("ts_ns=$0 cmd=$1 options=$2 resp=$3", ts_ns, cmd, options, resp);
  }
};

std::vector<NATSTraceRecord> GetNATSTraceRecords(
    const types::ColumnWrapperRecordBatch& record_batch, int pid) {
  std::vector<NATSTraceRecord> res;
  for (const auto& idx : FindRecordIdxMatchesPID(record_batch, nats_idx::kUPID, pid)) {
    res.push_back(
        NATSTraceRecord{record_batch[nats_idx::kTime]->Get<types::Time64NSValue>(idx).val,
                        std::string(record_batch[nats_idx::kCMD]->Get<types::StringValue>(idx)),
                        std::string(record_batch[nats_idx::kOptions]->Get<types::StringValue>(idx)),
                        std::string(record_batch[nats_idx::kResp]->Get<types::StringValue>(idx))});
  }
  return res;
}

auto EqualsNATSTraceRecord(std::string cmd, std::string options, std::string resp) {
  return AllOf(Field(&NATSTraceRecord::cmd, cmd),
               Field(&NATSTraceRecord::options, HasSubstr(options)),
               Field(&NATSTraceRecord::resp, resp));
}

// Tests that a series of commands issued by the test client were traced.
TEST_P(NATSTraceBPFTest, VerifyBatchedCommands) {
  StartTransferDataThread();

  std::vector<std::string> args;
  if (!GetParam()) {
    args = {"--ca="};
  }

  client_container_.Run(
      std::chrono::seconds{10},
      {absl::Substitute("--network=container:$0", server_container_.container_name())}, args);

  const int server_pid = server_container_.process_pid();

  client_container_.Wait();

  StopTransferDataThread();

  std::vector<TaggedRecordBatch> tablets = ConsumeRecords(SocketTraceConnector::kNATSTableNum);

  ASSERT_NOT_EMPTY_AND_GET_RECORDS(const types::ColumnWrapperRecordBatch& records, tablets);

  EXPECT_THAT(
      GetNATSTraceRecords(records, server_pid),
      UnorderedElementsAre(
          EqualsNATSTraceRecord("CONNECT", absl::Substitute(R"("tls_required":$0)", GetParam()),
                                ""),
          EqualsNATSTraceRecord("INFO", R"("host":"0.0.0.0","port":4222,"headers":true)", ""),
          EqualsNATSTraceRecord("SUB", R"({"sid":"1","subject":"foo"})", ""),
          EqualsNATSTraceRecord("MSG", R"({"payload":"Hello World","sid":"1","subject":"foo"})",
                                ""),
          EqualsNATSTraceRecord("PUB", R"({"payload":"Hello World","subject":"foo"})", ""),
          EqualsNATSTraceRecord("UNSUB", R"({"sid":"1"})", "")));
}

INSTANTIATE_TEST_SUITE_P(TLSandNonTLS, NATSTraceBPFTest, ::testing::Values(true, false));

}  // namespace stirling
}  // namespace px
