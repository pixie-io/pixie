#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/vizier/services/agent/pem/tracepoint_manager.h"

namespace pl {
namespace vizier {
namespace agent {

constexpr auto kUpdateInterval = std::chrono::seconds(2);

TracepointManager::TracepointManager(pl::event::Dispatcher* dispatcher, Info* agent_info,
                                     Manager::VizierNATSConnector* nats_conn,
                                     stirling::Stirling* stirling,
                                     table_store::TableStore* table_store,
                                     RelationInfoManager* relation_info_manager)
    : MessageHandler(dispatcher, agent_info, nats_conn),
      dispatcher_(dispatcher),
      nats_conn_(nats_conn),
      stirling_(stirling),
      table_store_(table_store),
      relation_info_manager_(relation_info_manager) {
  // Monitor is the background co-routine that monitors the state of the probes and sends
  // updates to the MDS.
  tracepoint_monitor_timer_ =
      dispatcher_->CreateTimer(std::bind(&TracepointManager::Monitor, this));
  // Kick off the background monitor.
  tracepoint_monitor_timer_->EnableTimer(kUpdateInterval);
  PL_UNUSED(nats_conn_);
}

Status TracepointManager::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  // The main purpose of handle message is to update the local state based on updates
  // from the MDS.
  if (!msg->has_tracepoint_message()) {
    return error::InvalidArgument("Can only handle probe requests");
  }

  const messages::TracepointMessage& tracepoint = msg->tracepoint_message();
  switch (tracepoint.msg_case()) {
    case messages::TracepointMessage::kRegisterTracepointRequest: {
      return HandleRegisterTracepointRequest(tracepoint.register_tracepoint_request());
    }
    case messages::TracepointMessage::kRemoveTracepointRequest: {
      return HandleRemoveTracepointRequest(tracepoint.remove_tracepoint_request());
    }
    default:
      LOG(ERROR) << "Unknown message type: " << tracepoint.msg_case() << " skipping";
  }
  return Status::OK();
}

Status TracepointManager::HandleRegisterTracepointRequest(
    const messages::RegisterTracepointRequest& req) {
  const std::string& name = req.tracepoint_deployment().name();
  PL_ASSIGN_OR_RETURN(auto id, ParseUUID(req.id()));
  auto program_copy =
      std::make_unique<stirling::dynamic_tracing::ir::logical::TracepointDeployment>(
          req.tracepoint_deployment());

  TracepointInfo info;
  info.name = name;
  info.current_state = statuspb::PENDING_STATE;
  info.expected_state = statuspb::RUNNING_STATE;
  info.last_updated_at = dispatcher_->GetTimeSource().MonotonicTime();

  // Poke Stirling to actually add the tracepoint.
  stirling_->RegisterTracepoint(id, std::move(program_copy));

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracepoints_[id] = std::move(info);
  }
  return Status::OK();
}

Status TracepointManager::HandleRemoveTracepointRequest(
    const messages::RemoveTracepointRequest& req) {
  PL_ASSIGN_OR_RETURN(auto id, ParseUUID(req.id()));
  std::lock_guard<std::mutex> lock(mu_);

  auto it = tracepoints_.find(id);
  if (it == tracepoints_.end()) {
    return error::NotFound("Tracepoint with ID: $0, not found", id.str());
  }

  it->second.expected_state = statuspb::TERMINATED_STATE;
  return stirling_->RemoveTracepoint(id);
}

std::string TracepointManager::DebugString() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::stringstream ss;
  auto now = std::chrono::steady_clock::now();
  ss << absl::Substitute("Tracepoint Manager Debug State:\n");
  ss << absl::Substitute("ID\tNAME\tCURRENT_STATE\tEXPECTED_STATE\tlast_updated\n");
  for (const auto& [id, tracepoint] : tracepoints_) {
    ss << absl::Substitute(
        "$0\t$1\t$2\t$3\t$4 seconds\n", id.str(), tracepoint.name,
        statuspb::LifeCycleState_Name(tracepoint.current_state),
        statuspb::LifeCycleState_Name(tracepoint.expected_state),
        std::chrono::duration_cast<std::chrono::seconds>(now - tracepoint.last_updated_at).count());
  }
  return ss.str();
}

void TracepointManager::Monitor() {
  std::lock_guard<std::mutex> lock(mu_);

  for (auto& [id, tracepoint] : tracepoints_) {
    auto s_or_publish = stirling_->GetTracepointInfo(id);
    statuspb::LifeCycleState current_state;
    // Get the latest current state according to stirling.
    if (s_or_publish.ok()) {
      current_state = statuspb::RUNNING_STATE;
    } else {
      switch (s_or_publish.code()) {
        case statuspb::FAILED_PRECONDITION:
          // Means the binary has not been found.
          current_state = statuspb::FAILED_STATE;
          break;
        case statuspb::RESOURCE_UNAVAILABLE:
          current_state = statuspb::PENDING_STATE;
          break;
        case statuspb::NOT_FOUND:
          // Means we didn't actually find the probe. If we requested termination,
          // it's because the probe has been removed.
          current_state = (tracepoint.expected_state == statuspb::TERMINATED_STATE)
                              ? statuspb::TERMINATED_STATE
                              : statuspb::UNKNOWN_STATE;
          break;
        default:
          current_state = statuspb::FAILED_STATE;
          break;
      }
    }

    if (current_state != statuspb::RUNNING_STATE &&
        tracepoint.expected_state == statuspb::TERMINATED_STATE) {
      current_state = statuspb::TERMINATED_STATE;
    }

    if (current_state == tracepoint.current_state) {
      // No state transition, nothing to do.
      continue;
    }

    // The following transitions are legal:
    // 1. Pending -> Terminated: Probe is stopped before starting.
    // 2. Pending -> Running : Probe starts up.
    // 3. Running -> Terminated: Probe is stopped.
    // 4. Running -> Failed: Probe got dettached because binary died.
    // 5. Failed -> Running: Probe started up because binary came back to life.
    //
    // In all cases we basically inform the MDS.
    // In the cases where we transtion to running, we need to update the schemas.

    Status probe_status = Status::OK();
    LOG(INFO) << absl::Substitute("Tracepoint[$0]::$1 has transitioned $2 -> $3", id.str(),
                                  tracepoint.name,
                                  statuspb::LifeCycleState_Name(tracepoint.current_state),
                                  statuspb::LifeCycleState_Name(current_state));
    // Check if running now, then update the schema.
    if (current_state == statuspb::RUNNING_STATE) {
      // We must have just transitioned into running. We try to apply the new schema.
      // If it fails we will trigger an error and report that to MDS.
      auto publish_pb = s_or_publish.ConsumeValueOrDie();
      auto s = UpdateSchema(publish_pb);
      if (!s.ok()) {
        current_state = statuspb::FAILED_STATE;
        probe_status = s;
      }
    } else {
      probe_status = s_or_publish.status();
    }

    tracepoint.current_state = current_state;
    // Update MDS with the latest status.
    pl::vizier::messages::VizierMessage msg;
    auto tracepoint_msg = msg.mutable_tracepoint_message();
    auto update_msg = tracepoint_msg->mutable_tracepoint_info_update();
    ToProto(agent_info()->agent_id, update_msg->mutable_agent_id());
    ToProto(id, update_msg->mutable_id());
    update_msg->set_state(tracepoint.current_state);
    probe_status.ToProto(update_msg->mutable_status());
    auto s = nats_conn_->Publish(msg);
    // TODO(zasgar/michelle): We should just maintain a global nats queue that we
    // can push into and ensure that we can keep retrying updates.
    if (!s.ok()) {
      LOG(ERROR) << "Failed to update nats";
    }
  }
  tracepoint_monitor_timer_->EnableTimer(kUpdateInterval);
}

Status TracepointManager::UpdateSchema(const stirling::stirlingpb::Publish& publish_pb) {
  auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);

  auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);

  // TODO(zasgar): Failure here can lead to an inconsistent schema state. We should
  // figure out how to handle this as part of the data model refactor project.
  for (const auto& relation_info : relation_info_vec) {
    if (!relation_info_manager_->HasRelation(relation_info.name)) {
      table_store_->AddTable(relation_info.id, relation_info.name,
                             table_store::Table::Create(relation_info.relation));
      PL_RETURN_IF_ERROR(relation_info_manager_->AddRelationInfo(relation_info));
    }
    // TODO(zasgar): We should verify that the case where table names are reused we have the
    // exact same relation.
  }
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_pb));
  return Status();
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
