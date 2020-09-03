#include "src/carnot/exec/grpc_sink_node.h"

#include <vector>

#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <sole.hpp>

#include "src/carnot/exec/test_utils.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/planpb/test_proto.h"
#include "src/carnot/udf/registry.h"
#include "src/carnotpb/carnot_mock.grpc.pb.h"
#include "src/common/testing/testing.h"
#include "src/common/uuid/uuid_utils.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace exec {

using carnotpb::MockResultSinkServiceStub;
using carnotpb::ResultSinkService;
using carnotpb::TransferResultChunkRequest;
using carnotpb::TransferResultChunkResponse;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using testing::proto::EqualsProto;

class GRPCSinkNodeTest : public ::testing::Test {
 public:
  GRPCSinkNodeTest() {
    func_registry_ = std::make_unique<udf::Registry>("test_registry");
    auto table_store = std::make_shared<table_store::TableStore>();
    exec_state_ = std::make_unique<ExecState>(
        func_registry_.get(), table_store,
        [this](const std::string&,
               const std::string&) -> std::unique_ptr<ResultSinkService::StubInterface> {
          auto s = std::make_unique<MockResultSinkServiceStub>();
          mock_ = s.get();
          return s;
        },
        sole::uuid4(), nullptr, [this](grpc::ClientContext*) { add_metadata_called_ = true; });

    table_store::schema::Relation rel({types::DataType::BOOLEAN, types::DataType::TIME64NS},
                                      {"col1", "time_"});
  }

 protected:
  std::unique_ptr<ExecState> exec_state_;
  std::unique_ptr<udf::Registry> func_registry_;
  MockResultSinkServiceStub* mock_;
  bool add_metadata_called_ = false;
};

constexpr char kExpectedInteralResult0[] = R"proto(
address: "localhost:1234"
query_id {
  data: "$0"
}
query_result {
  row_batch {
    cols {
      int64_data {
      }
    }
  }
  grpc_source_id: 0
}
)proto";

constexpr char kExpectedInteralResult1[] = R"proto(
address: "localhost:1234"
query_id {
  data: "$0"
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 1
      }
    }
    num_rows: 1
  }
  grpc_source_id: 0
}
)proto";

constexpr char kExpectedInteralResult2[] = R"proto(
address: "localhost:1234"
query_id {
  data: "$0"
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 2
        data: 2
      }
    }
    num_rows: 2
    eow: true
    eos: true
  }
  grpc_source_id: 0
}
)proto";

TEST_F(GRPCSinkNodeTest, internal_result) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink1PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  TransferResultChunkResponse resp;
  resp.set_success(true);

  std::vector<TransferResultChunkRequest> actual_protos(3);
  std::vector<std::string> expected_protos = {
      absl::Substitute(kExpectedInteralResult0, exec_state_->query_id().str()),
      absl::Substitute(kExpectedInteralResult1, exec_state_->query_id().str()),
      absl::Substitute(kExpectedInteralResult2, exec_state_->query_id().str()),
  };

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(3)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  for (auto i = 0; i < 3; ++i) {
    std::vector<types::Int64Value> data(i, i);
    auto rb = RowBatchBuilder(output_rd, i, /*eow*/ i == 2, /*eos*/ i == 2)
                  .AddColumn<types::Int64Value>(data)
                  .get();

    TransferResultChunkRequest expected_proto;
    tester.ConsumeNext(rb, 5, 0);
  }

  tester.Close();

  for (auto i = 0; i < 3; ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(expected_protos[i]));
  }

  EXPECT_FALSE(add_metadata_called_);
}

constexpr char kExpectedExteralResult0[] = R"proto(
address: "localhost:1234"
query_id {
  data: "$0"
}
query_result {
  row_batch {
    cols {
      int64_data {
      }
    }
  }
  table_name: "output_table_name"
}
)proto";

constexpr char kExpectedExteralResult1[] = R"proto(
address: "localhost:1234"
query_id {
  data: "$0"
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 1
      }
    }
    num_rows: 1
  }
  table_name: "output_table_name"
}
)proto";

constexpr char kExpectedExteralResult2[] = R"proto(
address: "localhost:1234"
query_id {
  data: "$0"
}
query_result {
  row_batch {
    cols {
      int64_data {
        data: 2
        data: 2
      }
    }
    num_rows: 2
    eow: true
    eos: true
  }
  table_name: "output_table_name"
}
)proto";

TEST_F(GRPCSinkNodeTest, external_result) {
  auto op_proto = planpb::testutils::CreateTestGRPCSink2PB();
  auto plan_node = std::make_unique<plan::GRPCSinkOperator>(1);
  auto s = plan_node->Init(op_proto.grpc_sink_op());
  RowDescriptor input_rd({types::DataType::INT64});
  RowDescriptor output_rd({types::DataType::INT64});

  google::protobuf::util::MessageDifferencer differ;

  auto tester = exec::ExecNodeTester<GRPCSinkNode, plan::GRPCSinkOperator>(
      *plan_node, output_rd, {input_rd}, exec_state_.get());

  TransferResultChunkResponse resp;
  resp.set_success(true);

  std::vector<TransferResultChunkRequest> actual_protos(3);
  std::vector<std::string> expected_protos = {
      absl::Substitute(kExpectedExteralResult0, exec_state_->query_id().str()),
      absl::Substitute(kExpectedExteralResult1, exec_state_->query_id().str()),
      absl::Substitute(kExpectedExteralResult2, exec_state_->query_id().str()),
  };

  auto writer = new grpc::testing::MockClientWriter<TransferResultChunkRequest>();

  EXPECT_CALL(*writer, Write(_, _))
      .Times(3)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[1]), Return(true)))
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[2]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));
  EXPECT_CALL(*mock_, TransferResultChunkRaw(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  for (auto i = 0; i < 3; ++i) {
    std::vector<types::Int64Value> data(i, i);
    auto rb = RowBatchBuilder(output_rd, i, /*eow*/ i == 2, /*eos*/ i == 2)
                  .AddColumn<types::Int64Value>(data)
                  .get();

    TransferResultChunkRequest expected_proto;
    tester.ConsumeNext(rb, 5, 0);
  }

  tester.Close();

  for (auto i = 0; i < 3; ++i) {
    EXPECT_THAT(actual_protos[i], EqualsProto(expected_protos[i]));
  }

  EXPECT_TRUE(add_metadata_called_);
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
