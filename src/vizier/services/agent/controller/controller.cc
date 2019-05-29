#include <arrow/pretty_print.h>
#include <gflags/gflags.h>
#include <google/protobuf/text_format.h>
#include <grpcpp/grpcpp.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "src/common/base/base.h"
#include "src/common/nats/nats.h"
#include "src/common/perf/perf.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/schema/utils.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/controller/controller.h"
#include "src/vizier/services/agent/controller/throwaway_dummy_data.h"

namespace pl {
namespace vizier {
namespace agent {

using types::DataType;

// The maximum size of the hostname, before we get an error.
constexpr uint64_t kMaxHostnameSize = 128;

// Retries for registration ACK.
constexpr auto kRegistrationPeriod = std::chrono::seconds(2);
constexpr uint32_t kRegistrationRetries = 5;

// The amount of time to wait for a hearbeat ack.
constexpr uint32_t kHeartbeatWaitMS = 5000;

// The number of seconds to wait before a timeout and retry. Note this is just
// the max time before it runs the next iteration and does not imply an error.
constexpr uint32_t kNextMessageTimeoutS = 5;

StatusOr<std::unique_ptr<Controller>> Controller::Create(
    sole::uuid agent_id, std::unique_ptr<QBStub> qb_stub, std::unique_ptr<carnot::Carnot> carnot,
    std::unique_ptr<stirling::Stirling> stirling,
    std::shared_ptr<table_store::TableStore> table_store,
    std::unique_ptr<VizierNATSConnector> nats_connector) {
  std::unique_ptr<Controller> controller(
      new Controller(agent_id, std::move(qb_stub), std::move(carnot), std::move(stirling),
                     std::move(table_store), std::move(nats_connector)));
  PL_RETURN_IF_ERROR(controller->Init());
  return controller;
}

Controller::Controller(sole::uuid agent_id, std::unique_ptr<QBStub> qb_stub,
                       std::unique_ptr<carnot::Carnot> carnot,
                       std::unique_ptr<stirling::Stirling> stirling,
                       std::shared_ptr<table_store::TableStore> table_store,
                       std::unique_ptr<VizierNATSConnector> nats_connector)
    : qb_stub_(std::move(qb_stub)),
      carnot_(std::move(carnot)),
      stirling_(std::move(stirling)),
      table_store_(std::move(table_store)),
      agent_id_(agent_id),
      nats_connector_(std::move(nats_connector)) {}

Status Controller::Init() {
  char hostname[kMaxHostnameSize];
  int err = gethostname(hostname, kMaxHostnameSize);
  if (err != 0) {
    return error::Unknown("Failed to get hostname");
  }
  hostname_ = hostname;
  LOG(INFO) << "Hostname: " << hostname_;

  if (stirling_) {
    // Register the Stirling Callback.
    stirling_->RegisterCallback(std::bind(&table_store::TableStore::AppendData, table_store_.get(),
                                          std::placeholders::_1, std::placeholders::_2));
  }

  PL_RETURN_IF_ERROR(nats_connector_->Connect());

  // Send the agent info.
  PL_RETURN_IF_ERROR(RegisterAgent());

  return Status::OK();
}

Status Controller::RegisterAgent() {
  if (nats_connector_->ApproximatePendingMessages() != 0) {
    // We already have messages in the queue. This should never happen since
    // messages to this ID should never have been sent before registration.
    // The agent will kill itself and hope that a restart will fix the issue.
    LOG(FATAL) << "Agent has messages before registration. Terminating.";
  }

  // Send the registration request.
  messages::VizierMessage req;
  auto register_request = req.mutable_register_agent_request();
  auto agent_info = register_request->mutable_info();
  ToProto(agent_id_, agent_info->mutable_agent_id());
  auto host_info = agent_info->mutable_host_info();
  *(host_info->mutable_hostname()) = hostname_;

  PL_RETURN_IF_ERROR(nats_connector_->Publish(req));

  // Wait for MDS to respond.
  uint32_t num_tries = 0;
  bool got_mds_response = false;
  do {
    auto msg = nats_connector_->GetNextMessage(kRegistrationPeriod);
    if (msg != nullptr) {
      got_mds_response = true;
      break;
    }
    ++num_tries;
  } while (num_tries < kRegistrationRetries);

  if (!got_mds_response) {
    LOG(FATAL) << "Failed to register agent. Terminating.";
  }

  return Status::OK();
}

void Controller::RunHeartbeat() {
  // Store the exponential moving average so we can report it.
  double hb_latency_moving_average = 0;
  double alpha = 0.25;

  while (keepAlive_) {
    // Send a heartbeat on a fixed interval.
    messages::VizierMessage req;
    auto hb = req.mutable_heartbeat();
    ToProto(agent_id_, hb->mutable_agent_id());

    int64_t current_time = CurrentTimeNS();
    hb->set_time(current_time);

    auto s = nats_connector_->Publish(req);
    if (!s.ok()) {
      LOG(ERROR) << absl::StrFormat("Failed to publish heartbeat (will retry): %s", s.msg());
      continue;
    }

    // Wait for the Heartbeat ACK. If no ack is received within the ACK timeout (multiple of ACK
    // interval), assume that connection has been lost. We should also make sure that the HB
    // interval is not exceeded.
    // TODO(zasgar): We need to add sequence numbers to heartbeat before we can make this work
    // reliably.
    std::unique_ptr<messages::VizierMessage> resp;
    incoming_heartbeat_queue_.wait_dequeue_timed(resp, std::chrono::milliseconds(kHeartbeatWaitMS));

    if (resp == nullptr) {
      LOG(FATAL) << "Failed to receive heartbeat ACK. terminating";
    }

    int64_t elapsed_time = std::max<int64_t>(0, CurrentTimeNS() - current_time);
    hb_latency_moving_average = alpha * hb_latency_moving_average + (1.0 - alpha) * elapsed_time;
    LOG_EVERY_N(INFO, 10) << absl::StrFormat("Heartbeat ACK latency moving average: %.2f ms",
                                             hb_latency_moving_average / 1.0E6);

    uint64_t sleep_interval_ms =
        static_cast<int64_t>(((kAgentHeartbeatIntervalSeconds * 1E9) - elapsed_time) / 1.0E6);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval_ms));
  }
}

Status Controller::ExecuteQuery(
    const messages::ExecuteQueryRequest& req,
    pl::vizier::services::query_broker::querybrokerpb::AgentQueryResponse* resp) {
  VLOG(1) << "Executing query: "
          << absl::StrFormat("id=%s, query=%s", ParseUUID(req.query_id()).ConsumeValueOrDie().str(),
                             req.query_str());
  CHECK(resp != nullptr);
  *resp->mutable_query_id() = req.query_id();

  {
    ScopedTimer query_timer("query timer");
    auto result_or_s = carnot_->ExecuteQuery(req.query_str(), CurrentTimeNS());
    if (!result_or_s.ok()) {
      *resp->mutable_status() = result_or_s.status().ToProto();
      return result_or_s.status();
    }
    PL_RETURN_IF_ERROR(result_or_s.ConsumeValueOrDie().ToProto(resp->mutable_query_result()));
  }
  *resp->mutable_status() = Status::OK().ToProto();

  return Status::OK();
}

Status Controller::Run() {
  // Start the heartbeat thread.
  // TODO(zasgar): Consider moving the thread creation outside this function.
  heartbeat_thread_ = std::make_unique<std::thread>(&Controller::RunHeartbeat, this);

  while (keepAlive_) {
    auto msg = nats_connector_->GetNextMessage(std::chrono::seconds(kNextMessageTimeoutS));
    if (msg == nullptr) {
      continue;
    }
    if (msg->has_execute_query_request()) {
      auto s = HandleExecuteQueryMessage(std::move(msg));
      if (!s.ok()) {
        // Ignore query execution failures, since they might hapeen for legitimate reasons.
        // TODO(zasgar) : Make sure failures are used for truly exceptional circumstances.
        LOG(ERROR) << absl::StrFormat("Failed to execute query... ignoring: %s", s.msg());
      }
    } else if (msg->has_heartbeat_ack() || msg->has_heartbeat_nack()) {
      PL_CHECK_OK(HandleHeartbeatMessage(std::move(msg)));
    } else {
      LOG(FATAL) << "An unknown message: " << msg->DebugString();
    }
  }

  if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
    heartbeat_thread_->join();
  }

  return Status::OK();
}

// Temporary and to be replaced by data table from Stirling and Executor
Status Controller::AddDummyTable(const std::string& name,
                                 std::shared_ptr<table_store::Table> table) {
  table_store_->AddTable(name, table);
  return Status::OK();
}

Status Controller::InitThrowaway() {
  // Add Dummy test data.
  PL_RETURN_IF_ERROR(
      AddDummyTable("hipster_data", pl::agent::FakeHipsterTable().ConsumeValueOrDie()));

  pl::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_pb));

  // This should eventually be done by subscribe requests.
  auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);
  for (const auto& relation_info : relation_info_vec) {
    PL_RETURN_IF_ERROR(
        table_store_->AddTable(relation_info.name, relation_info.id,
                               std::make_shared<table_store::Table>(relation_info.relation)));
  }
  return Status::OK();
}

Status Controller::Stop() {
  keepAlive_ = false;
  return Status::OK();
}

Status Controller::HandleHeartbeatMessage(std::unique_ptr<messages::VizierMessage> msg) {
  DCHECK(msg->has_heartbeat_ack() || msg->has_heartbeat_nack());
  if (msg->has_heartbeat_nack()) {
    // TODO(zasgar): Handle heartbeat NACKs.
    LOG(FATAL) << "Got a hearbeat NACK ..., this is currenttly unsupported";
  }
  incoming_heartbeat_queue_.enqueue(std::move(msg));
  return Status::OK();
}

Status Controller::HandleExecuteQueryMessage(std::unique_ptr<messages::VizierMessage> msg) {
  const auto& executor_query_req = msg->execute_query_request();
  pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultRequest req;

  auto s = ExecuteQuery(executor_query_req, req.mutable_result());
  if (!s.ok()) {
    LOG(ERROR) << absl::StrFormat("Query failed, reason: %s, query: %s", s.ToString(),
                                  executor_query_req.query_str());
  }

  // RPC the results to the query broker.
  pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultResponse resp;
  ToProto(agent_id_, req.mutable_agent_id());
  grpc::ClientContext context;
  auto query_response_status = qb_stub_->ReceiveAgentQueryResult(&context, req, &resp);
  if (!query_response_status.ok()) {
    LOG(ERROR) << absl::StrFormat(
        "Failed to send query response, code = %d, message = %s, details = %s",
        query_response_status.error_code(), query_response_status.error_message(),
        query_response_status.error_details());
  }

  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
