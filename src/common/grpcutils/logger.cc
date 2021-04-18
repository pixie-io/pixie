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

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <string>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/common/grpcutils/logger.h"

using grpc::experimental::InterceptionHookPoints;
using grpc::experimental::Interceptor;
using grpc::experimental::ServerRpcInfo;

namespace px {

class LoggingInterceptor : public grpc::experimental::Interceptor {
 public:
  explicit LoggingInterceptor(grpc::experimental::ServerRpcInfo* info) : info_(info) {}

  void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::POST_RECV_INITIAL_METADATA)) {
      start_time_ = std::chrono::high_resolution_clock::now();
    }

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_STATUS)) {
      status_ = methods->GetSendStatus();
    }

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::POST_RECV_CLOSE)) {
      auto stop_time = std::chrono::high_resolution_clock::now();
      auto duration = stop_time - start_time_;
      auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
      if (status_.ok()) {
        LOG(INFO) << absl::StrFormat("%s: %s", info_->method(), PrettyDuration(duration_ns));
      } else {
        LOG(ERROR) << absl::StrFormat("%s: %s %s", info_->method(), PrettyDuration(duration_ns),
                                      status_.error_message());
      }
    }

    methods->Proceed();
  }

 private:
  grpc::experimental::ServerRpcInfo* info_;
  grpc::Status status_;
  std::chrono::high_resolution_clock::time_point start_time_;
};

Interceptor* px::LoggingInterceptorFactory::CreateServerInterceptor(ServerRpcInfo* info) {
  return new LoggingInterceptor(info);
}

}  // namespace px
