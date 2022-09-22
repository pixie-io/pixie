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

#include "experimental/delve_connector/dlv_driver.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "src/common/base/base.h"

using ::px::Status;

DelveDriver::~DelveDriver() { Close(); }

Status DelveDriver::Connect(std::string host, int port) {
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_aton(host.c_str(), &server_addr.sin_addr);

  sockfd_ = socket(AF_INET, SOCK_STREAM, /*protocol*/ 0);
  if (sockfd_ <= 0) {
    return ::px::error::Internal("Failed to create socket, error message: $0", strerror(errno));
  }

  int retval = connect(sockfd_, reinterpret_cast<const struct sockaddr*>(&server_addr),
                       sizeof(struct sockaddr_in));
  if (retval != 0) {
    return ::px::error::Internal("Failed to connect, error message: $0", std::strerror(errno));
  }

  return Status::OK();
}

void DelveDriver::Init() {
  // Mimics what real Delve front-end does, for compatibility.
  SendRequest("RPCServer.SetApiVersion", R"([{"APIVersion":2}])");
  SendRequest("RPCServer.IsMulticlient", "[{}]");
  SendRequest("RPCServer.Recorded", "[{}]");
}

void DelveDriver::Close() {
  if (sockfd_ != -1) {
    SendRequest("RPCServer.Detach", R"([{"Kill":false}])");
    int retval = close(sockfd_);
    if (retval == 0) {
      sockfd_ = -1;
    }
  }
}

jsonrpcpp::response_ptr DelveDriver::SendRequest(std::string_view method, std::string_view params) {
  VLOG(1) << "---------------------------";

  static int id = 0;

  std::string req =
      absl::Substitute(R"({"method":"$0","params":$1,"id":$2})", method, params, id++);

  VLOG(1) << req;
  size_t bytes_sent = 0;
  while (bytes_sent < req.size()) {
    ssize_t len = write(sockfd_, req.data() + bytes_sent, req.size() - bytes_sent);
    CHECK_NE(len, -1);
    bytes_sent += len;
  }

  // Keep reading and gathering the response until we receive a newline character.
  std::string resp_buf;
  while (resp_buf.back() != '\n') {
    constexpr size_t kBufChunkSize = 1024;
    ssize_t pos = resp_buf.size();
    resp_buf.resize(resp_buf.size() + kBufChunkSize);
    ssize_t len = read(sockfd_, resp_buf.data() + pos, kBufChunkSize);
    CHECK_NE(len, -1);
    resp_buf.resize(resp_buf.size() - kBufChunkSize + len);
  }

  CHECK_EQ(resp_buf.back(), '\n');
  resp_buf.pop_back();

  // HACK: add jsonrpc to the JSON response from Delve back-end to keep jsonrpcpp happy.
  CHECK_EQ(resp_buf.back(), '}');
  resp_buf.pop_back();
  resp_buf += R"(, "jsonrpc": "2.0"})";

  // Parse the response and return the parsed form.
  jsonrpcpp::entity_ptr entity = jsonrpcpp::Parser::do_parse(resp_buf);
  CHECK(entity && entity->is_response());
  jsonrpcpp::response_ptr resp = std::dynamic_pointer_cast<jsonrpcpp::Response>(entity);
  VLOG(1) << resp->to_json().dump();
  return resp;
}

void DelveDriver::CreateBreakpoint(std::string_view symbol) {
  jsonrpcpp::response_ptr resp =
      SendRequest("RPCServer.FindLocation", absl::Substitute(R"([{"Loc":"$0"}])", symbol));
  // Other interesting FindLocation parameters (and their default values):
  //   "Scope":{"GoroutineID":-1,"Frame":0,"DeferredCall":0}
  CHECK(!resp->result().is_null());
  int64_t addr = resp->result()["Locations"][0]["pc"].get<int64_t>();
  LOG(INFO) << absl::Substitute("Address of $0 = $1", symbol, addr);

  SendRequest("RPCServer.CreateBreakpoint",
              absl::Substitute(R"([{"Breakpoint":{"addr":$0}}])", addr));
  // Other interesting CreateBreakpoint parameters (and their default values):
  //   "id":0
  //   "name":""
  //   "file":""
  //   "line":0
  //   "Cond":""
  //   "continue":false
  //   "traceReturn":false
  //   "goroutine":false
  //   "stacktrace":0
}

void DelveDriver::Continue() { SendRequest("RPCServer.Command", R"([{"name":"continue"}])"); }

Json DelveDriver::Eval(std::string_view expr) {
  constexpr std::string_view kScope = R"({"GoroutineID":-1,"Frame":0,"DeferredCall":0})";
  constexpr std::string_view kCfg =
      R"({"FollowPointers":true,"MaxVariableRecurse":1,"MaxStringLen":64,"MaxArrayValues":64,"MaxStructFields":-1})";
  auto r = SendRequest("RPCServer.Eval", absl::Substitute(R"([{"Scope":$0,"Expr":"$1","Cfg":$2}])",
                                                          kScope, expr, kCfg));
  if (r->result().is_null()) {
    return {};
  }
  return r->result()["Variable"]["value"];
}
