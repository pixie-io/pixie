#pragma once

#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/macros.h"

namespace pl {
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
                                        const GRPCReqType& req, std::vector<GRPCRespType>* resp) {
    ::grpc::ClientContext ctx;
    SetDeadline(&ctx, std::chrono::seconds(1));

    std::unique_ptr<::grpc::ClientReader<GRPCRespType>> reader(((*stub_).*fn)(&ctx, req));

    GRPCRespType feature;
    while (reader->Read(&feature)) {
      resp->push_back(std::move(feature));
    }
    return reader->Finish();
  }

 private:
  std::unique_ptr<typename GRPCServiceType::Stub> stub_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
