#include <glog/logging.h>
#include <grpcpp/test/mock_stream.h>
#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/macros.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/services/cloud_connector/cloud_connectorpb/service_mock.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

#include "src/vizier/services/shared/log/grpc_log_sink/grpc_log_sink.h"

namespace pl {
namespace vizier {
namespace services {
namespace shared {
namespace log {

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;

struct logMsg {
  const std::string log;
  const google::LogSeverity severity;
  explicit logMsg(const std::string& str, const google::LogSeverity sev = google::GLOG_INFO)
      : log(str), severity(sev) {}
};

void verifyMessage(const cloud_connector::LogMessage& expected,
                   const cloud_connector::LogMessage& actual) {
  EXPECT_EQ(expected.pod(), actual.pod());
  EXPECT_EQ(expected.svc(), actual.svc());
  auto expected_log = expected.log();
  auto actual_log = actual.log().substr(actual.log().size() - expected_log.size());
  EXPECT_EQ(expected_log, actual_log);
}

void testLog(std::unique_ptr<GRPCLogSink> sink, const std::vector<logMsg>& msgs,
             const std::vector<cloud_connector::TransferLogRequest>& expected_protos,
             const std::vector<cloud_connector::TransferLogRequest>& actual_protos) {
  time_t rawtime;
  time(&rawtime);

  for (const logMsg& msg : msgs) {
    sink->send(msg.severity, "", "", 20, localtime(&rawtime), msg.log.c_str(), msg.log.size());
  }

  sink->TerminateNetworkThread();

  for (size_t i = 0; i < expected_protos.size(); ++i) {
    EXPECT_EQ(expected_protos[i].batched_logs_size(), actual_protos[i].batched_logs_size())
        << ("Actual at " + std::to_string(i) + ": " + actual_protos[i].DebugString());
    for (auto j = 0; j < expected_protos[i].batched_logs_size(); ++j) {
      auto expectedlog = expected_protos[i].batched_logs(j);
      auto actuallog = actual_protos[i].batched_logs(j);
      verifyMessage(expectedlog, actuallog);
    }
  }
}

TEST(GRPCLogSink, error) {
  std::vector<logMsg> msgs = {logMsg("foo", google::GLOG_FATAL)};
  std::vector<cloud_connector::TransferLogRequest> expected_protos;
  cloud_connector::TransferLogRequest req;
  cloud_connector::LogMessage* batched = req.add_batched_logs();
  batched->set_pod("podname");
  batched->set_svc("svcname");
  batched->set_log(msgs[0].log);
  expected_protos.push_back(req);

  cloud_connector::TransferLogResponse resp;
  resp.set_ok(true);
  auto writer = new grpc::testing::MockClientWriter<cloud_connector::TransferLogRequest>();
  std::vector<cloud_connector::TransferLogRequest> actual_protos(expected_protos.size());

  EXPECT_CALL(*writer, Write(_, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&actual_protos[0]), Return(true)));

  EXPECT_CALL(*writer, WritesDone());
  EXPECT_CALL(*writer, Finish()).WillOnce(Return(grpc::Status::OK));

  auto mock = std::make_unique<cloud_connector::MockCloudConnectorServiceStub>();
  EXPECT_CALL(*mock, TransferLogRaw(_, _)).WillOnce(DoAll(SetArgPointee<1>(resp), Return(writer)));

  testLog(std::make_unique<GRPCLogSink>(std::move(mock), "podname", "svcname", 100,
                                        std::chrono::milliseconds(0)),
          msgs, expected_protos, actual_protos);
}

}  // namespace log
}  // namespace shared
}  // namespace services
}  // namespace vizier
}  // namespace pl
