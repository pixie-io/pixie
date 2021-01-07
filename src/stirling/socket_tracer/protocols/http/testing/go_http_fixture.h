#pragma once

#include <string>

#include "src/common/exec/subprocess.h"

namespace pl {
namespace stirling {
namespace testing {

class GoHTTPFixture {
 public:
  GoHTTPFixture() {
    constexpr char kClientPath[] =
        "src/stirling/socket_tracer/protocols/http/testing/go_http_client/go_http_client_/"
        "go_http_client";
    constexpr char kServerPath[] =
        "src/stirling/socket_tracer/protocols/http/testing/go_http_server/go_http_server_/"
        "go_http_server";

    client_path_ = pl::testing::BazelBinTestFilePath(kClientPath);
    server_path_ = pl::testing::BazelBinTestFilePath(kServerPath);
  }

  void LaunchServer() {
    ASSERT_OK(s_.Start({server_path_}));

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    ASSERT_OK(s_.Stdout(&port_str));
    ASSERT_TRUE(absl::SimpleAtoi(port_str, &s_port_));
    ASSERT_NE(0, s_port_);
  }

  void LaunchClient() {
    ASSERT_OK(
        c_.Start({client_path_, "-name=PixieLabs", absl::StrCat("-address=localhost:", s_port_)}));
    EXPECT_EQ(0, c_.Wait()) << "Client should exit normally.";
  }

  void ShutDown() {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
  }

  int client_pid() const { return c_.child_pid(); }
  int server_pid() const { return s_.child_pid(); }
  int server_port() const { return s_port_; }

 private:
  std::filesystem::path server_path_;
  std::filesystem::path client_path_;

  SubProcess c_;
  SubProcess s_;

  int s_port_ = -1;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
