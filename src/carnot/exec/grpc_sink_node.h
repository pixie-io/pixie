#pragma once

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/carnotpb/carnot.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

#include "src/carnotpb/carnot.grpc.pb.h"

namespace pl {
namespace carnot {
namespace exec {

class GRPCSinkNode : public SinkNode {
 public:
  GRPCSinkNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  Status CloseWriter();

  bool sent_eos_ = false;

  grpc::ClientContext context_;
  carnotpb::TransferResultChunkResponse response_;

  carnotpb::ResultSinkService::StubInterface* stub_;
  std::unique_ptr<grpc::ClientWriterInterface<carnotpb::TransferResultChunkRequest>> writer_;

  std::unique_ptr<plan::GRPCSinkOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
