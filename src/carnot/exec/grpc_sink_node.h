#pragma once

#include <grpcpp/grpcpp.h>

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnotpb/carnot.pb.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/carnotpb/carnot.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace carnot {
namespace exec {

class GRPCSinkNode : public SinkNode {
 public:
  GRPCSinkNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(
      const plan::Operator& plan_node, const table_store::schema::RowDescriptor& output_descriptor,
      const std::vector<table_store::schema::RowDescriptor>& input_descriptors) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  Status CloseWriter();

  bool sent_eos_ = false;

  grpc::ClientContext context_;
  carnotpb::RowBatchResponse response_;

  std::unique_ptr<carnotpb::KelvinService::StubInterface> stub_;
  std::unique_ptr<grpc::ClientWriterInterface<carnotpb::RowBatchRequest>> writer_;

  std::unique_ptr<plan::GrpcSinkOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
