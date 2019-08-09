#pragma once

#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>

#include "src/common/base/base.h"
#include "src/common/base/macros.h"

namespace pl {
namespace stirling {
namespace testing {

template <typename GRPCServiceType>
class GRPCStub {
 public:
  explicit GRPCStub(const std::string& endpoint) {
    stub_ = GRPCServiceType::NewStub(
        ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()));
    CHECK(stub_ != nullptr) << "Failed to create gRPC service stub.";
  }

  template <typename GRPCReqType, typename GRPCRespType>
  ::grpc::Status CallRPC(::grpc::Status (GRPCServiceType::Stub::*fn)(::grpc::ClientContext*,
                                                                     const GRPCReqType&,
                                                                     GRPCRespType*),
                         const GRPCReqType& req, GRPCRespType* resp) {
    ::grpc::ClientContext ctx;
    return ((*stub_).*fn)(&ctx, req, resp);
  }

 private:
  std::unique_ptr<typename GRPCServiceType::Stub> stub_;
};

}  // namespace testing
}  // namespace stirling
}  // namespace pl
