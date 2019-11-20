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
#include "src/common/system/system.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/schema/utils.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/controller/controller.h"

DEFINE_int32(agent_registration_attempts, 6,
             "The attempts made by an agent to connect with the broker, "
             "before giving up and crashing. Exponential backoffs are added between attempts. "
             "The initial backoff is 1 second, 6 attempts roughly gives 2^6-1 = 63 seconds.");

namespace pl {
namespace vizier {
namespace agent {

using ::pl::shared::k8s::metadatapb::ResourceUpdate;
using ::pl::types::DataType;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResponse;

// The maximum size of the hostname, before we get an error.
constexpr int kMaxHostnameSize = 128;

// Retries for registration ACK.
constexpr std::chrono::seconds kRegistrationPeriod{2};

// The amount of time to wait for a hearbeat ack.
constexpr std::chrono::milliseconds kHeartbeatWaitMillis{5000};

// The number of seconds to wait before a timeout and retry. Note this is just
// the max time before it runs the next iteration and does not imply an error.
constexpr std::chrono::seconds kNextMessageTimeoutSecs{5};

constexpr int64_t kAgentHeartbeatIntervalNS = 5'000'000'000;

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

Controller::~Controller() {
  // Don't expect this to happen.
  // But if it does, give a good chunk of time for final queries to terminate.
  // Note that kill signals go through a Signal handler, not through here,
  // and those cases will have a different (smaller) amount of time to live.
  static constexpr std::chrono::milliseconds kTimeout{10000};
  Status s = Stop(kTimeout);
  LOG_IF(WARNING, !s.ok()) << s.msg();
}

Status Controller::Init() {
  char hostname[kMaxHostnameSize];
  int err = gethostname(hostname, kMaxHostnameSize);
  if (err != 0) {
    return error::Unknown("Failed to get hostname");
  }
  hostname_ = hostname;
  LOG(INFO) << "Hostname: " << hostname_;

  // The first step is to connect to stats and register the agent.
  // Downstream dependencies like stirling/carnot depend on knowing
  // ASID and metadata state, which is only available after registration is
  // complete.
  if (nats_connector_ == nullptr) {
    LOG(WARNING) << "NATS is not configured, skip connecting. Stirling and Carnot might not behave "
                    "as expected because of this.";
  } else {
    PL_RETURN_IF_ERROR(nats_connector_->Connect());
    // Send the agent info.
    PL_RETURN_IF_ERROR(RegisterAgent());
  }

  mds_manager_ =
      std::make_unique<pl::md::AgentMetadataStateManager>(asid_, pl::system::Config::GetInstance());
  relation_info_manager_ = std::make_unique<RelationInfoManager>();

  if (stirling_) {
    // Register the Stirling Callback.
    stirling_->RegisterCallback(std::bind(&table_store::TableStore::AppendData, table_store_.get(),
                                          std::placeholders::_1, std::placeholders::_2,
                                          std::placeholders::_3));

    // Register the metadata callback for Stirling.
    stirling_->RegisterAgentMetadataCallback(std::bind(
        &pl::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager_.get()));
  }

  // Register the Carnot callback for metadata.
  carnot_->RegisterAgentMetadataCallback(
      std::bind(&pl::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager_.get()));

  return Status::OK();
}

void Controller::StartHelperThreads() {
  heartbeat_thread_ = std::make_unique<std::thread>(&Controller::RunHeartbeat, this);

  mds_thread_ = std::make_unique<std::thread>([this]() {
    while (keep_alive_) {
      VLOG(1) << "State Update";
      ECHECK_OK(mds_manager_->PerformMetadataStateUpdate());
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  });
}

Status Controller::RegisterAgent() {
  if (nats_connector_->ApproximatePendingMessages() != 0) {
    LOG(FATAL) << "Agent has messages before registration. Terminating. "
                  "We already have messages in the queue. This should never happen since "
                  "messages to this ID should never have been sent before registration. "
                  "The agent will kill itself and hope that a restart will fix the issue.";
  }

  // Send the registration request.
  messages::VizierMessage req;
  auto agent_info = req.mutable_register_agent_request()->mutable_info();
  ToProto(agent_id_, agent_info->mutable_agent_id());
  auto host_info = agent_info->mutable_host_info();
  host_info->set_hostname(hostname_);

  PL_RETURN_IF_ERROR(nats_connector_->Publish(req));

  // Wait for MDS to respond.
  bool got_mds_response = false;
  auto wait_period = std::chrono::seconds(1);
  for (int num_attempts = 0; num_attempts < FLAGS_agent_registration_attempts; ++num_attempts) {
    auto msg = nats_connector_->GetNextMessage(kRegistrationPeriod);
    if (msg != nullptr &&
        msg->msg_case() == messages::VizierMessage::MsgCase::kRegisterAgentResponse) {
      asid_ = msg->register_agent_response().asid();
      got_mds_response = true;
      break;
    }
    std::this_thread::sleep_for(wait_period);
    // TODO(yzhao): A fancier mechanism would be to draw wait period uniformly from
    // [previous wait period, next doubled wait period]. That theoretically is the safest option to
    // avoid flash crowd behavior.
    wait_period *= 2;
  }

  if (!got_mds_response) {
    LOG(FATAL) << "Failed to register agent. Terminating.";
  }

  return Status::OK();
}

void Controller::ProcessPIDStartedEvent(const pl::md::PIDStartedEvent& ev,
                                        messages::AgentUpdateInfo* update_info) {
  auto* process_info = update_info->add_process_created();

  auto upid = ev.pid_info.upid();
  auto* mu_upid = process_info->mutable_upid();
  mu_upid->set_high(absl::Uint128High64(upid.value()));
  mu_upid->set_low(absl::Uint128Low64(upid.value()));

  // TODO(zasgar/michelle): We should remove pid since it's already in UPID.
  process_info->set_pid(upid.pid());
  process_info->set_start_timestamp_ns(ev.pid_info.start_time_ns());
  process_info->set_cmdline(ev.pid_info.cmdline());
  process_info->set_cid(ev.pid_info.cid());
}

void Controller::ProcessPIDTerminatedEvent(const pl::md::PIDTerminatedEvent& ev,
                                           messages::AgentUpdateInfo* update_info) {
  auto process_info = update_info->add_process_terminated();
  process_info->set_stop_timestamp_ns(ev.stop_time_ns);

  auto upid = ev.upid;
  // TODO(zasgar): Move this into ToProto function.
  auto* mu_pid = process_info->mutable_upid();
  mu_pid->set_high(absl::Uint128High64(upid.value()));
  mu_pid->set_low(absl::Uint128Low64(upid.value()));
}

void Controller::ConsumeAgentPIDUpdates(messages::AgentUpdateInfo* update_info) {
  while (auto pid_event = mds_manager_->GetNextPIDStatusEvent()) {
    switch (pid_event->type) {
      case pl::md::PIDStatusEventType::kStarted: {
        auto* ev = static_cast<pl::md::PIDStartedEvent*>(pid_event.get());
        ProcessPIDStartedEvent(*ev, update_info);
        break;
      }
      case pl::md::PIDStatusEventType::kTerminated: {
        auto* ev = static_cast<pl::md::PIDTerminatedEvent*>(pid_event.get());
        ProcessPIDTerminatedEvent(*ev, update_info);
        break;
      }
      default:
        CHECK(0) << "Unknown PID event";
    }
  }
}

Status Controller::HandleMDSUpdates(const messages::MetadataUpdateInfo& update_info) {
  for (const auto& update : update_info.updates()) {
    PL_RETURN_IF_ERROR(mds_manager_->AddK8sUpdate(std::make_unique<ResourceUpdate>(update)));
  }
  return Status::OK();
}

Status Controller::WaitForHelperThreads() {
  if (keep_alive_) {
    return error::Internal(
        "Wait for threads called while keep alive is set. This is disallowed as it might cause "
        "deadlock.");
  }

  if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
    heartbeat_thread_->join();
  }

  if (mds_thread_ && mds_thread_->joinable()) {
    mds_thread_->join();
  }
  return Status::OK();
}

void Controller::RunHeartbeat() {
  // Store the exponential moving average so we can report it.
  double hb_latency_moving_average = 0;
  double alpha = 0.25;

  while (keep_alive_) {
    if (nats_connector_ == nullptr) {
      // TODO(yzhao): LOG_EVERY_N has a known data race: https://github.com/google/glog/issues/212.
      // We'd want to switch this to LOG_EVERY_N(WARNING, 100) for better visibility.
      VLOG(1) << "NATS is not configured, sleep for 1 second ...";
      std::this_thread::sleep_for(std::chrono::seconds{1});
      continue;
    }
    // Send a heartbeat on a fixed interval.
    messages::VizierMessage req;
    auto hb = req.mutable_heartbeat();
    ToProto(agent_id_, hb->mutable_agent_id());

    int64_t current_time = CurrentTimeNS();
    hb->set_time(current_time);

    auto* update_info = hb->mutable_update_info();

    // Grab the PID updates and put it into the heartbeat message.
    ConsumeAgentPIDUpdates(update_info);

    // Grab current schema available.
    relation_info_manager_->AddSchemaToUpdateInfo(update_info);

    VLOG(2) << "Sending heartbeat message: " << req.DebugString();
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
    incoming_heartbeat_queue_.wait_dequeue_timed(resp, kHeartbeatWaitMillis);

    if (resp == nullptr) {
      LOG(FATAL) << "Failed to receive heartbeat ACK, terminating ...";
    }

    if (resp->heartbeat_ack().has_update_info()) {
      PL_CHECK_OK(HandleMDSUpdates(resp->heartbeat_ack().update_info()));
    }

    const int64_t elapsed_time_ns = std::max<int64_t>(0, CurrentTimeNS() - current_time);
    hb_latency_moving_average = alpha * hb_latency_moving_average + (1.0 - alpha) * elapsed_time_ns;
    LOG_EVERY_N(INFO, 10) << absl::StrFormat("Heartbeat ACK latency moving average: %.2f ms",
                                             hb_latency_moving_average / 1.0E6);

    int64_t sleep_interval_ns = kAgentHeartbeatIntervalNS - elapsed_time_ns;
    if (sleep_interval_ns > 0) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_interval_ns));
    }
  }
}

Status Controller::ExecuteQuery(const messages::ExecuteQueryRequest& req,
                                AgentQueryResponse* resp) {
  auto query_id = ParseUUID(req.query_id()).ConsumeValueOrDie();
  LOG(INFO) << absl::StrFormat("Executing query: id=%s, query=%s", query_id.str(), req.query_str());
  CHECK(resp != nullptr);
  *resp->mutable_query_id() = req.query_id();

  {
    ScopedTimer query_timer("query timer");
    StatusOr<carnot::CarnotQueryResult> result_or_s;
    if (req.has_plan()) {
      result_or_s = carnot_->ExecutePlan(req.plan(), query_id);
    } else {
      result_or_s = carnot_->ExecuteQuery(req.query_str(), query_id, CurrentTimeNS());
    }
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
  StartHelperThreads();

  running_ = true;

  while (keep_alive_) {
    if (nats_connector_ == nullptr || qb_stub_ == nullptr) {
      VLOG(1) << "NATS or Query Broker is not configured, sleep for 1 second ...";
      std::this_thread::sleep_for(std::chrono::seconds{1});
      continue;
    }
    auto msg = nats_connector_->GetNextMessage(kNextMessageTimeoutSecs);
    if (msg == nullptr) {
      continue;
    }
    if (msg->has_execute_query_request()) {
      auto s = HandleExecuteQueryMessage(std::move(msg));
      if (!s.ok()) {
        // Ignore query execution failures, since they might hapeen for legitimate reasons.
        // TODO(zasgar) : Make sure failures are used for truly exceptional circumstances.
        LOG(ERROR) << absl::Substitute("Failed to execute query, ignoring: $0", s.msg());
      }
    } else if (msg->has_heartbeat_ack() || msg->has_heartbeat_nack()) {
      PL_CHECK_OK(HandleHeartbeatMessage(std::move(msg)));
    } else {
      // TODO(zasgar): Figure out why we are getting some unknown messages.
      LOG(ERROR) << "Got an unknown message: " << msg->DebugString();
    }
  }

  PL_RETURN_IF_ERROR(WaitForHelperThreads());

  running_ = false;

  return Status::OK();
}

Status Controller::InitThrowaway() {
  pl::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_pb));

  // This should eventually be done by subscribe requests.
  auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);
  PL_RETURN_IF_ERROR(relation_info_manager_->UpdateRelationInfo(relation_info_vec));
  for (const auto& relation_info : relation_info_vec) {
    PL_RETURN_IF_ERROR(table_store_->AddTable(relation_info.id, relation_info.name,
                                              table_store::Table::Create(relation_info.relation)));
  }
  return Status::OK();
}

Status Controller::Stop(std::chrono::milliseconds timeout) {
  keep_alive_ = false;
  if (stirling_) {
    stirling_->Stop();
  }

  // Wait for a limited amount of time for main thread to stop processing.
  std::chrono::time_point expiration_time = std::chrono::steady_clock::now() + timeout;
  while (running_ && std::chrono::steady_clock::now() < expiration_time) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }

  return Status::OK();
}

Status Controller::HandleHeartbeatMessage(std::unique_ptr<messages::VizierMessage> msg) {
  DCHECK(msg->has_heartbeat_ack() || msg->has_heartbeat_nack());
  if (msg->has_heartbeat_nack()) {
    // TODO(zasgar): Handle heartbeat NACKs.
    LOG(FATAL) << "Got a hearbeat NACK ..., this is currently unsupported";
  }
  incoming_heartbeat_queue_.enqueue(std::move(msg));
  return Status::OK();
}

Status Controller::HandleExecuteQueryMessage(std::unique_ptr<messages::VizierMessage> msg) {
  const auto& executor_query_req = msg->execute_query_request();
  pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultRequest req;

  auto s = ExecuteQuery(executor_query_req, req.mutable_result());
  if (!s.ok()) {
    LOG(ERROR) << absl::Substitute("Query failed, reason: $0, query: $1", s.ToString(),
                                   executor_query_req.query_str());
  }

  // RPC the results to the query broker.
  pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultResponse resp;
  ToProto(agent_id_, req.mutable_agent_id());
  grpc::ClientContext context;
  auto query_response_status = qb_stub_->ReceiveAgentQueryResult(&context, req, &resp);
  if (!query_response_status.ok()) {
    LOG(ERROR) << absl::Substitute(
        "Failed to send query response, code = $0, message = $1, details = $2",
        query_response_status.error_code(), query_response_status.error_message(),
        query_response_status.error_details());
  }

  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
