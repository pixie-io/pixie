/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <absl/container/flat_hash_map.h>
#include <prometheus/registry.h>
#include "src/carnot/plan/plan.h"
#include "src/vizier/services/agent/shared/manager/manager.h"

namespace px {
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
  ExecuteQueryMessageHandler(px::event::Dispatcher* dispatcher, Info* agent_info,
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
  absl::flat_hash_map<sole::uuid, px::event::RunnableAsyncTaskUPtr> running_queries_;

  prometheus::Gauge& num_queries_in_flight_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
