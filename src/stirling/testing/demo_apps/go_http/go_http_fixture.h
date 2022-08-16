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

#pragma once

#include <string>

#include "src/common/exec/subprocess.h"

namespace px {
namespace stirling {
namespace testing {

class GoHTTPFixture {
 public:
  GoHTTPFixture() {
    constexpr std::string_view kClientPath =
        "src/stirling/testing/demo_apps/go_http/go_http_client/go_http_client_/go_http_client";
    constexpr std::string_view kServerPath =
        "src/stirling/testing/demo_apps/go_http/go_http_server/go_http_server_/go_http_server";

    client_path_ = px::testing::BazelRunfilePath(kClientPath);
    server_path_ = px::testing::BazelRunfilePath(kServerPath);
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

  void LaunchGetClient() {
    ASSERT_OK(c_.Start({
        client_path_,
        absl::StrCat("-address=localhost:", s_port_),
        "-reqType=get",
        "-name=PixieLabs",
    }));
    EXPECT_EQ(0, c_.Wait()) << "Client should exit normally.";
  }

  void LaunchPostClient() {
    ASSERT_OK(c_.Start({
        client_path_,
        absl::StrCat("-address=localhost:", s_port_),
        "-reqType=post",
        "-reqSize=131072",
    }));
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
}  // namespace px
