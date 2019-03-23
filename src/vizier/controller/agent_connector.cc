#include "src/vizier/controller/agent_connector.h"
#include "absl/strings/str_format.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"

namespace pl {
namespace vizier {

// The time after a missing heartbeat that the agent is deemed unhealthy.
const int64_t kMissingHeartbeatBeforeUnhealthy = 30 * 1000000000LL;  // 30 seconds.

AgentConnector::AgentConnector(const sole::uuid &agent_id, const RegisterAgentRequest &req,
                               AgentReaderWriter *stream)
    : stream_(stream), agent_id_(agent_id) {
  agent_info_ = req.agent_info();
}

Status AgentConnector::RunMessageFetchLoop() {
  AgentToVizierMessage msg;
  while (stream_->Read(&msg)) {
    switch (msg.msg_case()) {
      case AgentToVizierMessage::kHeartbeat:
        HandleAgentHeartBeat(msg.heartbeat());
        break;
      case AgentToVizierMessage::kQueryResponse:
        HandleQueryResponse(msg.query_response());
        break;
      default:
        LOG(ERROR) << "Unhandled message. Ignoring ..." << msg.DebugString();
    }
  }

  return Status::OK();
}

void AgentConnector::HandleAgentHeartBeat(const HeartBeat &heartbeat) {
  // TODO(zasgar): PL-432 We should add a sequence ID to the heartbeat.
  PL_UNUSED(heartbeat);
  VLOG(1) << absl::StrFormat("Agent Heartbeat : %s", agent_id_.str());
  VizierToAgentMessage ack_msg;

  auto current_time = CurrentTimeNS();
  ack_msg.mutable_heart_beat_ack()->set_time(current_time);

  // Update internal state.
  last_heartbeat_ts_ = current_time;
  agent_state_ = AGENT_STATE_HEALTHY;

  if (!stream_->Write(ack_msg)) {
    LOG(ERROR) << "Failed to send heartbeat ack message";
  }
}

void AgentConnector::UpdateAgentState() {
  if ((CurrentTimeNS() - last_heartbeat_ts_) > kMissingHeartbeatBeforeUnhealthy) {
    agent_state_ = AGENT_STATE_UNRESPONSIVE;
  }
}

void AgentConnector::GetAgentStatus(AgentStatus *status) {
  CHECK(status != nullptr);
  *status->mutable_info() = agent_info_;
  status->set_last_heartbeat_ns(last_heartbeat_ts_);
  status->set_state(agent_state_);
}

Status AgentConnector::ExecuteQuery(const sole::uuid &query_id, const std::string &query_str) {
  std::lock_guard<std::mutex> lock(query_lock_);
  if (query_in_progress_) {
    // TODO(zasgar): We should allow for multiple queries to queue up, which will involve adding
    // a query completion queue.
    return error::ResourceUnavailable("Only a single query at a time is supported on the agent.");
  }
  current_query_id_ = query_id;
  query_in_progress_ = true;

  VizierToAgentMessage msg;
  auto query_req = msg.mutable_query_request();

  ToProto(query_id, query_req->mutable_query_id());
  *query_req->mutable_query_str() = query_str;

  // send query to agent (this will block until write completes).
  if (!stream_->Write(msg)) {
    auto err =
        absl::StrFormat("Failed to send query: %s, query_str=\n%s", query_id.str(), query_str);
    LOG(ERROR) << err;
    return error::Unknown(err);
  }
  return Status::OK();
}

Status AgentConnector::GetQueryResults(const sole::uuid &query_id,
                                       AgentQueryResponse *query_response) {
  // Check query ID and bail early if another query is already in flight.
  {
    std::lock_guard<std::mutex> lock(query_lock_);
    if (query_id != current_query_id_) {
      return error::InvalidArgument(
          "Asking for query results from a query that did not get executed.");
    }
  }

  // Wait for query completion.
  // TODO(zasgar): Handle hangups and errors from the agent.
  {
    std::unique_lock<std::mutex> lock(query_lock_);
    while ((query_response_.query_id().data().empty()) ||
           ParseUUID(query_response_.query_id()).ConsumeValueOrDie() != query_id) {
      query_cv_.wait(lock);
    }

    // At this point we have the query response.
    query_response->MergeFrom(query_response_);
    // Clear the UUID and query state.
    ClearUUID(&current_query_id_);
    query_in_progress_ = false;
  }

  return Status::OK();
}

void AgentConnector::HandleQueryResponse(const AgentQueryResponse &query_response) {
  std::lock_guard<std::mutex> lock(query_lock_);
  query_response_ = query_response;
  query_cv_.notify_all();
}

}  // namespace vizier
}  // namespace pl
