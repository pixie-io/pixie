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

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/macros.h"

namespace px {
namespace stirling {
namespace testing {

inline std::shared_ptr<::grpc::Channel> CreateInsecureGRPCChannel(const std::string& endpoint) {
  return ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials());
}

inline void SetDeadline(::grpc::ClientContext* ctx, std::chrono::seconds secs) {
  std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + secs;
  ctx->set_deadline(deadline);
}

template <typename GRPCServiceType>
class GRPCStub {
 public:
  explicit GRPCStub(const std::string& endpoint) {
    stub_ = GRPCServiceType::NewStub(
        ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()));
    CHECK(stub_ != nullptr) << "Failed to create gRPC service stub.";
  }

  explicit GRPCStub(std::shared_ptr<::grpc::Channel> grpc_channel) {
    stub_ = GRPCServiceType::NewStub(grpc_channel);
    CHECK(stub_ != nullptr) << "Failed to create gRPC service stub.";
  }

  template <typename GRPCReqType, typename GRPCRespType>
  ::grpc::Status CallRPC(::grpc::Status (GRPCServiceType::Stub::*fn)(::grpc::ClientContext*,
                                                                     const GRPCReqType&,
                                                                     GRPCRespType*),
                         const GRPCReqType& req, GRPCRespType* resp) {
    ::grpc::ClientContext ctx;
    SetDeadline(&ctx, std::chrono::seconds(1));
    return ((*stub_).*fn)(&ctx, req, resp);
  }

  // Based on https://grpc.io/docs/tutorials/basic/c/.
  template <typename GRPCReqType, typename GRPCRespType>
  ::grpc::Status CallServerStreamingRPC(std::unique_ptr<::grpc::ClientReader<GRPCRespType>> (
                                            GRPCServiceType::Stub::*fn)(::grpc::ClientContext*,
                                                                        const GRPCReqType&),
                                        const GRPCReqType& req, std::vector<GRPCRespType>* resps) {
    ::grpc::ClientContext ctx;
    SetDeadline(&ctx, std::chrono::seconds(1));

    std::unique_ptr<::grpc::ClientReader<GRPCRespType>> reader(((*stub_).*fn)(&ctx, req));

    GRPCRespType resp;
    while (reader->Read(&resp)) {
      resps->push_back(std::move(resp));
    }
    return reader->Finish();
  }

  template <typename GRPCReqType, typename GRPCRespType>
  ::grpc::Status CallBidirStreamingRPC(
      std::unique_ptr<::grpc::ClientReaderWriter<GRPCReqType, GRPCRespType>> (
          GRPCServiceType::Stub::*fn)(::grpc::ClientContext*),
      const std::vector<GRPCReqType>& reqs, std::vector<GRPCRespType>* resps) {
    ::grpc::ClientContext ctx;
    SetDeadline(&ctx, std::chrono::seconds(1));

    std::shared_ptr<::grpc::ClientReaderWriter<GRPCReqType, GRPCRespType>> stream(
        ((*stub_).*fn)(&ctx));

    std::thread writer([stream, reqs]() {
      for (const GRPCReqType& req : reqs) {
        stream->Write(req);
      }
      stream->WritesDone();
    });

    GRPCRespType resp;
    while (stream->Read(&resp)) {
      resps->push_back(std::move(resp));
    }
    writer.join();
    return stream->Finish();
  }

 private:
  std::unique_ptr<typename GRPCServiceType::Stub> stub_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
