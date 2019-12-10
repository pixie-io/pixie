#include "src/vizier/services/agent/manager/exec.h"

#include <memory>
#include <utility>

#include "src/common/base/base.h"
#include "src/common/event/task.h"
#include "src/common/perf/perf.h"
#include "src/vizier/services/agent/manager/manager.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/services/query_broker/querybrokerpb/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace vizier {
namespace agent {

using ::pl::event::AsyncTask;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResponse;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultRequest;
using ::pl::vizier::services::query_broker::querybrokerpb::AgentQueryResultResponse;

class ExecuteQueryMessageHandler::ExecuteQueryTask : public AsyncTask {
 public:
  ExecuteQueryTask(ExecuteQueryMessageHandler* h, const Info* agent_info, carnot::Carnot* carnot,
                   Manager::QueryBrokerServiceSPtr qb_stub,
                   std::unique_ptr<messages::VizierMessage> msg)
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

    auto s = ExecuteQueryInternal((qb_stub_ == nullptr) ? nullptr : res_req.mutable_result());
    if (!s.ok()) {
      LOG(ERROR) << absl::Substitute("Query failed, reason: $0, query: $1", s.ToString(),
                                     req_.query_str());
    }

    if (req_.plan().distributed() && agent_info_->capabilities.collects_data()) {
      // In distributed mode only non data collecting nodes send data.
      return;
    }

    CHECK(qb_stub_ != nullptr);

    // RPC the results to the query broker.
    AgentQueryResultResponse res_resp;
    ToProto(agent_info_->agent_id, res_req.mutable_agent_id());
    grpc::ClientContext context;
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
    LOG(INFO) << absl::Substitute("Executing query: id=$0, distributed=$1", query_id_.str(),
                                  req_.plan().distributed());
    VLOG(1) << absl::Substitute("Query Plan: $0=$1", query_id_.str(), req_.plan().DebugString());

    {
      ScopedTimer query_timer(absl::Substitute("query timer: id=$0", query_id_.str()));
      StatusOr<carnot::CarnotQueryResult> result_or_s;
      if (req_.has_plan()) {
        result_or_s = carnot_->ExecutePlan(req_.plan(), query_id_);
      } else {
        result_or_s = carnot_->ExecuteQuery(req_.query_str(), query_id_, CurrentTimeNS());
      }
      if (resp == nullptr) {
        return result_or_s.status();
      }

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
  Manager::QueryBrokerServiceSPtr qb_stub_;

  std::unique_ptr<messages::VizierMessage> msg_;
  const messages::ExecuteQueryRequest& req_;
  sole::uuid query_id_;
};

ExecuteQueryMessageHandler::ExecuteQueryMessageHandler(pl::event::Dispatcher* dispatcher,
                                                       Info* agent_info,
                                                       Manager::VizierNATSConnector* nats_conn,
                                                       Manager::QueryBrokerServiceSPtr qb_stub,
                                                       carnot::Carnot* carnot)
    : MessageHandler(dispatcher, agent_info, nats_conn), qb_stub_(qb_stub), carnot_(carnot) {}

Status ExecuteQueryMessageHandler::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  // Create a task and run it on the threadpool.
  auto task =
      std::make_unique<ExecuteQueryTask>(this, agent_info(), carnot_, qb_stub_, std::move(msg));

  auto query_id = task->query_id();
  auto runnable = dispatcher()->CreateAsyncTask(std::move(task));
  auto runnable_ptr = runnable.get();
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
