#include <google/protobuf/util/message_differencer.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <sole.hpp>

#include "src/carnot/exec/exec_node_mock.h"
#include "src/carnot/exec/grpc_sink_node.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid_utils.h"

namespace pl {
namespace carnot {
namespace exec {

using carnotpb::KelvinService;
using carnotpb::MockKelvinServiceStub;
using carnotpb::RowBatchRequest;
using carnotpb::RowBatchResponse;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

class GrpcSinkNodeTest : public ::testing::Test {
 public:
  GrpcSinkNodeTest() {
    udf_registry_ = std::make_unique<udf::ScalarUDFRegistry>("test_registry");
    uda_registry_ = std::make_unique<udf::UDARegistry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(
        udf_registry_.get(), uda_registry_.get(), table_store,
        [this](const std::string&) -> std::unique_ptr<KelvinService::StubInterface> {
          auto s = std::make_unique<MockKelvinServiceStub>();
          mock_ = s.get();
          return s;
        },
        sole::uuid4());

    table_store::schema::Relation rel({types::DataType::BOOLEAN, types::DataType::TIME64NS},
                                      {"col1", "time_"});
  }

 protected:
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::UDARegistry> uda_registry_;
  std::unique_ptr<udf::ScalarUDFRegistry> udf_registry_;
  MockKelvinServiceStub* mock_;
};

TEST_F(GrpcSinkNodeTest, basic) {
  auto op_proto = planpb::testutils::CreateTestGrpcSink1PB();
  auto plan_node = std::make_unique<plan::GrpcSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GrpcSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  RowBatchResponse resp;
  resp.set_success(true);

  std::vector<RowBatchRequest> actual_protos(3);
  std::vector<RowBatchRequest> expected_protos(3);

  auto writer = new grpc::testing::MockClientWriter<RowBatchRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(3)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferRowBatchRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  for (auto i = 0; i < 3; ++i) {
    std::vector<types::Int64Value> data(i, i);
    auto rb = RowBatchBuilder(output_rd, i, /*eow*/ i == 2, /*eos*/ i == 2)
                  .AddColumn<types::Int64Value>(data)
                  .get();

    RowBatchRequest expected_proto;
    expected_proto.set_address(plan_node->address());
    expected_proto.set_destination_id(plan_node->destination_id());
    ToProto(exec_state_->query_id(), expected_proto.mutable_query_id());
    EXPECT_OK(rb.ToProto(expected_proto.mutable_row_batch()));
    expected_protos[i] = expected_proto;
    tester.ConsumeNext(rb, 5, 0);
  }

  tester.Close();

  for (auto i = 0; i < 3; ++i) {
    EXPECT_TRUE(differ.Compare(expected_protos[i], actual_protos[i]));
  }
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
