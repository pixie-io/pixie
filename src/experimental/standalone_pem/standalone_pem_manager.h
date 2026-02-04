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
#include <utility>

#include "src/carnot/carnot.h"
#include "src/common/event/event.h"
#include "src/experimental/standalone_pem/sink_server.h"
#include "src/experimental/standalone_pem/tracepoint_manager.h"
#include "src/experimental/standalone_pem/vizier_server.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/stirling.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/services/agent/shared/base/base_manager.h"
#include "src/vizier/services/agent/shared/base/info.h"

namespace px {
namespace vizier {
namespace agent {

class StandalonePEMManager : public BaseManager {
 public:
  template <typename... Args>
  static StatusOr<std::unique_ptr<BaseManager>> Create(Args&&... args) {
    auto m = std::unique_ptr<StandalonePEMManager>(
        new StandalonePEMManager(std::forward<Args>(args)...));
    PX_RETURN_IF_ERROR(m->Init());
    return std::unique_ptr<BaseManager>(std::move(m));
  }

  StandalonePEMManager() = delete;
  StandalonePEMManager(sole::uuid agent_id, std::string_view host_ip, int port);

  Status Run() final;
  Status Init();
  Status Stop(std::chrono::milliseconds timeout) final;

 private:
  int port_;
  Status InitSchemas();
  Status StopImpl(std::chrono::milliseconds);

  // The time system to use (real or simulated).
  std::unique_ptr<px::event::TimeSystem> time_system_;

  px::event::APIUPtr api_;
  px::event::DispatcherUPtr dispatcher_;

  Info info_;

  bool stop_called_ = false;
  // The controller is still running. Force stopping will cause un-graceful termination.
  std::atomic<bool> running_ = false;

  std::shared_ptr<table_store::TableStore> table_store_;

  // Factory context for vizier functions.
  funcs::VizierFuncFactoryContext func_context_;

  std::unique_ptr<carnot::Carnot> carnot_;
  std::unique_ptr<stirling::Stirling> stirling_;

  std::unique_ptr<StandaloneGRPCResultSinkServer> results_sink_server_;
  std::unique_ptr<VizierGRPCServer> vizier_grpc_server_;

  std::unique_ptr<px::md::AgentMetadataStateManager> mds_manager_;
  // The timer to manage metadata updates.
  px::event::TimerUPtr metadata_update_timer_;

  // Tracepoints
  std::unique_ptr<TracepointManager> tracepoint_manager_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
