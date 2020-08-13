#include "src/vizier/services/agent/manager/exec.h"

#include <memory>
#include <string>
#include <utility>

#include <jwt/jwt.hpp>

#include "src/common/base/base.h"
#include "src/common/event/task.h"
#include "src/common/perf/perf.h"
#include "src/vizier/services/agent/manager/manager.h"

#include "src/vizier/services/query_broker/querybrokerpb/service.grpc.pb.h"

DECLARE_string(jwt_signing_key);

namespace pl {
namespace vizier {
namespace agent {

constexpr auto kRPCResultTimeout = std::chrono::seconds(2);

using ::pl::event::AsyncTask;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResponse;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultRequest;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultResponse;

std::string GenerateServiceToken() {
  jwt::jwt_object obj{jwt::params::algorithm("HS256")};
  obj.add_claim("iss", "PL");
  obj.add_claim("aud", "service");
  obj.add_claim("jti", sole::uuid4().str());
  obj.add_claim("iat", std::chrono::system_clock::now());
  obj.add_claim("nbf", std::chrono::system_clock::now() - std::chrono::seconds{60});
  obj.add_claim("exp", std::chrono::system_clock::now() + std::chrono::seconds{60});
  obj.add_claim("sub", "service");
  obj.add_claim("Scopes", "service");
  obj.add_claim("ServiceID", "kelvin");
  obj.secret(FLAGS_jwt_signing_key);
  return obj.signature();
}

class ExecuteQueryMessageHandler::ExecuteQueryTask : public AsyncTask {
 public:
  ExecuteQueryTask(ExecuteQueryMessageHandler* h, const Info* agent_info, carnot::Carnot* carnot,
                   QueryBrokerServiceSPtr qb_stub, std::unique_ptr<messages::VizierMessage> msg)
      : parent_(h),
        agent_info_(agent_info),
        carnot_(carnot),
        qb_stub_(qb_stub),
        msg_(std::move(msg)),
        req_(msg_->execute_query_request()),
        query_id_(ParseUUID(req_.query_id()).ConsumeValueOrDie()) {}

  sole::uuid query_id() { return query_id_; }

  void Work() override {
    AgentQueryResultRequest res_req;
    auto contains_batch_result_or_s = PlanContainsBatchResults(req_.plan());
    if (!contains_batch_result_or_s.ok()) {
      LOG(ERROR) << absl::Substitute("Query failed, reason: $0, plan: $1",
                                     contains_batch_result_or_s.status().msg(),
                                     req_.plan().DebugString());
    }

    bool send_batch_result = qb_stub_ != nullptr && contains_batch_result_or_s.ConsumeValueOrDie();

    auto s = ExecuteQueryInternal(send_batch_result ? res_req.mutable_result() : nullptr);
    if (!s.ok()) {
      LOG(ERROR) << absl::Substitute("Query failed, reason: $0, plan: $1", s.ToString(),
                                     req_.plan().DebugString());
    }

    if (!send_batch_result) {
      // In distributed mode only non data collecting nodes send data.
      // TODO(zasgar/philkuz/michelle): We should actually just code in the Querybroker address into
      // the plan and remove the hardcoding here.
      return;
    }

    CHECK(qb_stub_ != nullptr);

    // RPC the results to the query broker.
    AgentQueryResultResponse res_resp;
    ToProto(agent_info_->agent_id, res_req.mutable_agent_id());
    grpc::ClientContext context;
    std::string token = GenerateServiceToken();
    context.AddMetadata("authorization", absl::Substitute("bearer $0", token));
    // This timeout ensures that we don't get a hang.
    context.set_deadline(std::chrono::system_clock::now() + kRPCResultTimeout);
    auto query_response_status = qb_stub_->ReceiveAgentQueryResult(&context, res_req, &res_resp);
    if (!query_response_status.ok()) {
      LOG(ERROR) << absl::Substitute(
          "Failed to send query response, code = $0, message = $1, details = $2",
          query_response_status.error_code(), query_response_status.error_message(),
          query_response_status.error_details());
    }
  }

  void Done() override { parent_->HandleQueryExecutionComplete(query_id_); }

 private:
  Status ExecuteQueryInternal(AgentQueryResponse* resp) {
    LOG(INFO) << absl::Substitute("Executing query: id=$0", query_id_.str());
    VLOG(1) << absl::Substitute("Query Plan: $0=$1", query_id_.str(), req_.plan().DebugString());

    {
      ScopedTimer query_timer(absl::Substitute("query timer: id=$0", query_id_.str()));
      StatusOr<carnot::CarnotQueryResult> result_or_s;
      result_or_s = carnot_->ExecutePlan(req_.plan(), query_id_, req_.analyze());

      if (resp == nullptr) {
        return result_or_s.status();
      }

      // TODO(nserrino): Deprecate this logic when Kelvin executes all queries in streaming mode.
      *resp->mutable_query_id() = req_.query_id();
      if (!result_or_s.ok()) {
        *resp->mutable_status() = result_or_s.status().ToProto();
        return result_or_s.status();
      }
      PL_RETURN_IF_ERROR(result_or_s.ConsumeValueOrDie().ToProto(resp->mutable_query_result()));
    }
    *resp->mutable_status() = Status::OK().ToProto();
    return Status::OK();
  }

  ExecuteQueryMessageHandler* parent_;
  const Info* agent_info_;
  carnot::Carnot* carnot_;
  QueryBrokerServiceSPtr qb_stub_;

  std::unique_ptr<messages::VizierMessage> msg_;
  const messages::ExecuteQueryRequest& req_;
  sole::uuid query_id_;
};

ExecuteQueryMessageHandler::ExecuteQueryMessageHandler(pl::event::Dispatcher* dispatcher,
                                                       Info* agent_info,
                                                       Manager::VizierNATSConnector* nats_conn,
                                                       QueryBrokerServiceSPtr qb_stub,
                                                       carnot::Carnot* carnot)
    : MessageHandler(dispatcher, agent_info, nats_conn), qb_stub_(qb_stub), carnot_(carnot) {}

Status ExecuteQueryMessageHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  // Create a task and run it on the threadpool.
  auto task =
      std::make_unique<ExecuteQueryTask>(this, agent_info(), carnot_, qb_stub_, std::move(msg));

  auto query_id = task->query_id();
  auto runnable = dispatcher()->CreateAsyncTask(std::move(task));
  auto runnable_ptr = runnable.get();
  LOG(INFO) << "Queries in flight: " << running_queries_.size();
  running_queries_[query_id] = std::move(runnable);
  runnable_ptr->Run();

  return Status::OK();
}

void ExecuteQueryMessageHandler::HandleQueryExecutionComplete(sole::uuid query_id) {
  // Upon completion of the query, we makr the runnable task for deletion.
  auto node = running_queries_.extract(query_id);
  if (node.empty()) {
    LOG(ERROR) << "Attempting to delete non-existent query: " << query_id.str();
    return;
  }
  dispatcher()->DeferredDelete(std::move(node.mapped()));
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
