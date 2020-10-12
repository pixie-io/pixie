#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/base/base.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/stirling/testing/common.h"
#include "src/stirling/testing/socket_trace_bpf_test_fixture.h"

namespace pl {
namespace stirling {

using ::pl::stirling::testing::SocketTraceBPFTest;
using ::pl::testing::BazelBinTestFilePath;

// A DNS server using the bind9 DNS server image.
class DNSServerContainer : public ContainerRunner {
 public:
  DNSServerContainer()
      : ContainerRunner(BazelBinTestFilePath(kBazelImageTar), kInstanceNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kBazelImageTar =
      "src/stirling/testing/dns2/dns_server_image.tar";
  static constexpr std::string_view kInstanceNamePrefix = "dns_server";
  static constexpr std::string_view kReadyMessage = "all zones loaded";
};

class DNSTraceTest : public SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  DNSTraceTest() {
    // Run the bind DNS server.
    // The container runner will make sure it is in the ready state before unblocking.
    // Stirling will run after this unblocks, as part of SocketTraceBPFTest SetUp().
    // Note that this step will make an access to docker hub to download the bind image.
    PL_CHECK_OK(container_.Run(150, {}));
  }
  ~DNSTraceTest() { container_.Stop(); }

  DNSServerContainer container_;
};

//-----------------------------------------------------------------------------
// Test Scenarios
//-----------------------------------------------------------------------------

TEST_F(DNSTraceTest, capture) {
  // Sleep an additional second, just to be safe.
  sleep(1);

  // Run dig to generate a DNS request.
  // Run it through bash, and return the PID, so we can use it to filter captured results.
  std::string cmd =
      absl::StrFormat("docker exec %s sh -c 'dig @127.0.0.1 server.dnstest.com & echo $! && wait'",
                      container_.container_name());
  ASSERT_OK_AND_ASSIGN(std::string out, pl::Exec(cmd));
  LOG(INFO) << out;
}

}  // namespace stirling
}  // namespace pl
