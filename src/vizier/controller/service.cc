#include "src/vizier/controller/service.h"

#include <memory>
#include <utility>

#include <libcuckoo/cuckoohash_map.hh>
#include <taskflow/taskflow.hpp>

namespace pl {
namespace vizier {

grpc::Status VizierServiceImpl::ServeAgent(::grpc::ServerContext *, AgentReaderWriter *stream) {
  VLOG(1) << "Agent Connected: Serve Agent Request";
  AgentToVizierMessage agent_req;
  if (!stream->Read(&agent_req)) {
    LOG(ERROR) << "Failed to read message";
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "Failed to read message from agent. Hanging up!");
  }

  if (agent_req.msg_case() != AgentToVizierMessage::kRegisterRequest) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Expected register request");
  }

  // We have a register agent request. Create a new agent connector.
  const auto &register_agent_req = agent_req.register_request();
  auto agent_id = ParseUUID(agent_req.register_request().agent_id()).ConsumeValueOrDie();
  auto agent_controller = std::make_unique<AgentConnector>(agent_id, register_agent_req, stream);
  if (agents_.contains(agent_id)) {
    // TODO(zasgar): Handler cases where the agent temporarily goes away and comes back.
    LOG(ERROR)
        << "Agent already exists, this means that the agent likely re-connected. Not handeled yet.";
    return grpc::Status(grpc::INTERNAL, "Attempt to re-register agent failed");
  }
  auto *agent_controller_ptr = agent_controller.get();
  agents_.insert(agent_id, std::move(agent_controller));

  // Loop waiting for new messages.
  auto agent_status = agent_controller_ptr->RunMessageFetchLoop();
  if (!agent_status.ok()) {
    return grpc::Status(grpc::StatusCode::INTERNAL, agent_status.msg());
  }
  return grpc::Status::OK;
}

grpc::Status VizierServiceImpl::GetAgentInfo(::grpc::ServerContext *,
                                             const ::pl::vizier::AgentInfoRequest *,
                                             ::pl::vizier::AgentInfoResponse *response) {
  DCHECK(response != nullptr);
  // Read the agents table.
  auto lt = agents_.lock_table();
  for (const auto &kv : lt) {
    kv.second->GetAgentStatus(response->add_info());
  }
  lt.unlock();
  return grpc::Status::OK;
}

grpc::Status VizierServiceImpl::ExecuteQuery(::grpc::ServerContext *,
                                             const ::pl::vizier::QueryRequest *req,
                                             ::pl::vizier::VizierQueryResponse *resp) {
  LOG(INFO) << "Execute Query Request: " << req->DebugString();
  auto query_id_or_status = ParseUUID(req->query_id());
  if (!query_id_or_status.ok()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, query_id_or_status.msg());
  }
  auto query_id = query_id_or_status.ConsumeValueOrDie();
  auto query_str = req->query_str();

  // A task is created for each agent to perform the query execution and wait for
  // results. Any agent that returns data will fill out the output proto.
  // TODO(zasgar): Handle agent hangs/disconnects.
  tf::Taskflow tf;
  // Read the agents table.
  auto lt = agents_.lock_table();
  for (const auto &kv : lt) {
    auto *agent = kv.second.get();
    auto agent_id = agent->agent_id();
    if (agent->IsHealthy()) {
      auto *agent_resp_ptr = resp->add_responses();
      // Create the actual task. We bind by value since we want to capture the pointer values in
      // this loop and references to them.
      tf.emplace([=]() mutable {
        ToProto(agent->agent_id(), agent_resp_ptr->mutable_agent_id());
        auto *agent_resp = agent_resp_ptr->mutable_response();

        auto exec_query_status = agent->ExecuteQuery(query_id, query_str);
        if (!exec_query_status.ok()) {
          exec_query_status.ToProto(agent_resp->mutable_status());
          LOG(ERROR) << absl::StrFormat("agent: %s, Failed to execute query: %s", agent_id.str(),
                                        exec_query_status.msg());
          return;
        }

        auto query_result_status = agent->GetQueryResults(query_id, agent_resp);
        if (!query_result_status.ok()) {
          query_result_status.ToProto(agent_resp->mutable_status());
          LOG(ERROR) << absl::StrFormat("agent: %s, Failed to get query results: %s",
                                        agent_id.str(), query_result_status.msg());
          return;
        }
      });
    }
  }
  lt.unlock();

  // Execute and wait.
  tf.wait_for_all();

  // At this point the proto is completely filled out.
  return grpc::Status::OK;
}

}  // namespace vizier
}  // namespace pl
