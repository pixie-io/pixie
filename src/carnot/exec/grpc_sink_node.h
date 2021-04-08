#pragma once

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "src/carnot/carnotpb/carnot.pb.h"
#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

#include "src/carnot/carnotpb/carnot.grpc.pb.h"

namespace pl {
namespace carnot {
namespace exec {

constexpr std::chrono::milliseconds kDefaultConnectionCheckTimeoutMS{2000};
// Max request size is 1MB minus 16KB (about 1%) to account for differences between the public
// and private query result data structure size. For example, an extra string field for table ID
// is added to the public query result data structure.
constexpr size_t kMetadataMargin = 16 * 1024;
constexpr size_t kMaxBatchSize = 1024 * 1024 - kMetadataMargin;
// BatchSizeFactor is the size of kMaxBatchSize to split into to assure that we limit number of
// splits. We must split batches across row lines, not byte lines. Row batches aren't guaranteed to
// be uniformly distributed, so splitting a rowbatch will likely lead to one part of the split being
// larger than the other. This parameter can be tuned in the future depending on what we learn about
// the distributions of the row batches.
constexpr float kBatchSizeFactor = 0.5;

class GRPCSinkNode : public SinkNode {
 public:
  GRPCSinkNode() = default;
  virtual ~GRPCSinkNode() = default;

  // Used to check the downstream connection after connection_check_timeout_ has elapsed.
  Status OptionallyCheckConnection(ExecState* exec_state);

  void testing_set_connection_check_timeout(const std::chrono::milliseconds& timeout) {
    connection_check_timeout_ = timeout;
  }
  const std::chrono::time_point<std::chrono::system_clock>& testing_last_send_time() const {
    return last_send_time_;
  }

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;
  Status SplitAndSendBatch(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                           size_t parent_index, size_t request_size);

 private:
  Status CloseWriter(ExecState* exec_state);

  bool cancelled_ = true;

  grpc::ClientContext context_;
  carnotpb::TransferResultChunkResponse response_;

  carnotpb::ResultSinkService::StubInterface* stub_;
  std::unique_ptr<grpc::ClientWriterInterface<carnotpb::TransferResultChunkRequest>> writer_;

  std::unique_ptr<plan::GRPCSinkOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;

  std::chrono::milliseconds connection_check_timeout_ = kDefaultConnectionCheckTimeoutMS;
  std::chrono::time_point<std::chrono::system_clock> last_send_time_ =
      std::chrono::system_clock::now();
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
