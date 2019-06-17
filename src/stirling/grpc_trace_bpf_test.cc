#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/subprocess/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/stirling/socket_trace_connector.h"

namespace pl {
namespace stirling {

using ::testing::SizeIs;

TEST(GRPCTraceBPFTest, TestGolangGrpcService) {
  constexpr char kBaseDir[] = "src/stirling/testing";
  std::string s_path =
      TestEnvironment::PathToTestDataFile(absl::StrCat(kBaseDir, "/go_greeter_server"));
  std::string c_path =
      TestEnvironment::PathToTestDataFile(absl::StrCat(kBaseDir, "/go_greeter_client"));
  SubProcess s({s_path});
  EXPECT_OK(s.Start());

  // TODO(yzhao): We have to install probes after starting server. Otherwise we will run into
  // failures when detaching them. This might be relevant to probes are inherited by child process
  // when fork() and execvp().
  std::unique_ptr<SourceConnector> connector =
      SocketTraceConnector::Create("socket_trace_connector");
  ASSERT_OK(connector->Init());

  SubProcess c({c_path, "-name=PixieLabs", "-once"});
  EXPECT_OK(c.Start());
  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";
  s.Kill();
  EXPECT_EQ(9, s.Wait()) << "Server should have been killed.";

  auto* socket_trace_connector = dynamic_cast<SocketTraceConnector*>(connector.get());
  ASSERT_NE(nullptr, socket_trace_connector);

  const int kTableNum = 2;
  socket_trace_connector->ReadPerfBuffer(kTableNum);

  EXPECT_THAT(socket_trace_connector->TestOnlyHTTP2Streams(), SizeIs(2));
  EXPECT_OK(connector->Stop());
}

}  // namespace stirling
}  // namespace pl
