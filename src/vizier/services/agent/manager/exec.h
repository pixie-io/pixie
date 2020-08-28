#pragma once

#include <memory>

#include <absl/container/flat_hash_map.h>
#include "src/carnot/plan/plan.h"
#include "src/vizier/services/agent/manager/manager.h"
#include "src/vizier/services/query_broker/querybrokerpb/service.grpc.pb.h"

namespace pl {
namespace vizier {
namespace agent {

/**
 * ExecuteQueryMessageHandler takes execute query results and performs them.
 * If a qb_stub is specified the results will also be RPCd to the query broker,
 * otherwise only query execution is performed.
 *
 * This class runs all of it's work on a thread pool and tracks pending queries internally.
 */
class ExecuteQueryMessageHandler : public Manager::MessageHandler {
 public:
  ExecuteQueryMessageHandler() = delete;
  ExecuteQueryMessageHandler(pl::event::Dispatcher* dispatcher, Info* agent_info,
                             Manager::VizierNATSConnector* nats_conn, carnot::Carnot* carnot);
  ~ExecuteQueryMessageHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

 protected:
  /**
   * HandleQueryExecutionComplete can be called by the async task to signal that work has been
   * completed.
   */
  virtual void HandleQueryExecutionComplete(sole::uuid query_id);

 private:
  // Forward declare private task class.
  class ExecuteQueryTask;

  carnot::Carnot* carnot_;

  // Map from query_id -> Running query task.
  absl::flat_hash_map<sole::uuid, pl::event::RunnableAsyncTaskUPtr> running_queries_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
