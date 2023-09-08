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
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "src/carnot/carnot.h"
#include "src/common/base/base.h"
#include "src/common/event/event.h"
#include "src/common/event/nats.h"
#include "src/common/metrics/memory_metrics.h"
#include "src/common/system/kernel_version.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/metadata.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/shared/base/base_manager.h"
#include "src/vizier/services/agent/shared/base/info.h"
#include "src/vizier/services/agent/shared/manager/chan_cache.h"
#include "src/vizier/services/agent/shared/manager/relation_info_manager.h"

#include "src/vizier/services/metadata/metadatapb/service.grpc.pb.h"

namespace px {
namespace vizier {
namespace agent {

/**
 * The maximum number of entries and the maximum false positive error rate to use for
 * the metadata filter that is stored on this agent to track entities.
 */
constexpr int64_t kMetadataFilterMaxEntries = 10000;
constexpr double kMetadataFilterMaxErrorRate = 0.01;

/**
 * The length of time to wait in between channel cache garbage collection events
 */
constexpr auto kChanCacheCleanupChansionPeriod = std::chrono::minutes(5);
/**
 * The length of time to allow a channel to be idle before we delete the channel. See more
 * documentation in chan_cache.h.
 */
constexpr auto kChanIdleGracePeriod = std::chrono::minutes(1);

constexpr auto kTableStoreCompactionPeriod = std::chrono::minutes(1);

constexpr auto kMemoryMetricsCollectPeriod = std::chrono::minutes(1);

constexpr auto kMetricsPushPeriod = std::chrono::minutes(1);

// Generates a service bearer token for authenticated requests.
std::string GenerateServiceToken();

// Adds service token to a GPRC context for authentication.
void AddServiceTokenToClientContext(grpc::ClientContext* ctx);

// Forward-declare
class HeartbeatMessageHandler;
class RegistrationHandler;

/**
 * Manager is the shared code and common interface for the entity responsible for managing the
 * sub-components of a pixie agent. The base version has a table store, carnot and metadata system.
 * This version can be extended to add more sub-components.
 */
class Manager : public BaseManager {
 public:
  using VizierNATSConnector = px::event::NATSConnector<px::vizier::messages::VizierMessage>;
  using MsgCase = messages::VizierMessage::MsgCase;
  using MDSService = services::metadata::MetadataService;
  using MDSServiceSPtr = std::shared_ptr<Manager::MDSService::Stub>;
  using MDTPService = services::metadata::MetadataTracepointService;
  using MDTPServiceSPtr = std::shared_ptr<Manager::MDTPService::Stub>;
  using ResultSinkStub = px::carnotpb::ResultSinkService::StubInterface;

  Manager() = delete;
  virtual ~Manager() = default;

  // Forward decleration to prevent circular dependency on MessageHandler.
  class MessageHandler;

  Status Run() final;

  Status Stop(std::chrono::milliseconds timeout) final;

 protected:
  // Protect constructor since we need to use Init on this class.
  Manager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
          int grpc_server_port, services::shared::agent::AgentCapabilities capabilities,
          services::shared::agent::AgentParameters parameters, std::string_view nats_url,
          std::string_view mds_url, system::KernelVersion kernel_version);
  Status Init();

  Status RegisterMessageHandler(MsgCase c, std::shared_ptr<MessageHandler> handler,
                                bool override = false);

  // ************************************************************
  // Interfaces that need to be implemented for the derived variants
  // of the agent.
  // ************************************************************

  /**
   * InitImpl is called after all the Init of this class is complete.
   */
  virtual Status InitImpl() = 0;

  /**
   * StopImpl is called after all the Stop function of this class is complete.
   */
  virtual Status StopImpl(std::chrono::milliseconds timeout) = 0;

  /**
   * PostRegisterHookImpl is called after agent registration.
   */
  virtual Status PostRegisterHookImpl() = 0;

  // APIs for the derived classes to reference the state of the agent.
  table_store::TableStore* table_store() { return table_store_.get(); }
  px::md::AgentMetadataStateManager* mds_manager() { return mds_manager_.get(); }
  RelationInfoManager* relation_info_manager() { return relation_info_manager_.get(); }
  px::event::Dispatcher* dispatcher() { return dispatcher_.get(); }
  carnot::Carnot* carnot() { return carnot_.get(); }
  const Info* info() const { return &info_; }
  Info* info() { return &info_; }
  VizierNATSConnector* agent_nats_connector() { return agent_nats_connector_.get(); }

  // Kelvin and PEMs use different selectors to request k8s updates.
  virtual std::string k8s_update_selector() const = 0;

 private:
  std::unique_ptr<ResultSinkStub> ResultSinkStubGenerator(const std::string& remote_addr,
                                                          const std::string& ssl_targetname);
  void NATSMessageHandler(VizierNATSConnector::MsgType msg);
  Status RegisterBackgroundHelpers();
  Status PostRegisterHook(uint32_t asid);
  Status ReregisterHook();
  Status PostReregisterHook(uint32_t asid);

  static constexpr char kAgentSubTopicPattern[] = "Agent/$0";
  static constexpr char kAgentPubTopic[] = "UpdateAgent";
  static constexpr char kK8sSubTopicPattern[] = "K8sUpdates/$0";
  static constexpr char kK8sPubTopic[] = "MissingMetadataRequests";
  static constexpr char kMetricsPubTopic[] = "Metrics";

  // Message handlers are registered per type of Vizier message.
  // same message handler can be used for multiple different types of messages.
  absl::flat_hash_map<MsgCase, std::shared_ptr<MessageHandler>> message_handlers_;
  void HandleMessage(std::unique_ptr<messages::VizierMessage> msg);

  // The timer to manage metadata updates.
  px::event::TimerUPtr metadata_update_timer_;

  bool stop_called_ = false;

  // The data structure recording the metadata stored on this agent.
  std::unique_ptr<md::AgentMetadataFilter> agent_metadata_filter_;
  // Chan caches active connections to other Agents. Methods are all threadsafe.
  std::unique_ptr<ChanCache> chan_cache_;
  // The timer that runs the garbage collection routine.
  px::event::TimerUPtr chan_cache_garbage_collect_timer_;

  // A pointer to the heartbeat handler for reregistration hooks.
  std::shared_ptr<HeartbeatMessageHandler> heartbeat_handler_;
  std::shared_ptr<RegistrationHandler> registration_handler_;

  std::shared_ptr<grpc::ChannelCredentials> grpc_channel_creds_;

  // The time system to use (real or simulated).
  std::unique_ptr<px::event::TimeSystem> time_system_;
  px::event::APIUPtr api_;

  Info info_;
  px::event::DispatcherUPtr dispatcher_;
  const std::string nats_addr_;
  // NATS connector for subscribing to and publishing agent updates.
  std::unique_ptr<VizierNATSConnector> agent_nats_connector_;
  // NATS connector for subscribing to and requesting k8s updates.
  std::unique_ptr<VizierNATSConnector> k8s_nats_connector_;

  // The controller is still running. Force stopping will cause un-graceful termination.
  std::atomic<bool> running_ = false;

  // The base agent contains the following components.
  std::unique_ptr<carnot::Carnot> carnot_;
  std::shared_ptr<table_store::TableStore> table_store_;
  std::unique_ptr<px::md::AgentMetadataStateManager> mds_manager_;
  std::unique_ptr<RelationInfoManager> relation_info_manager_;

  std::shared_ptr<grpc::Channel> mds_channel_;
  // Factory context for vizier functions.
  funcs::VizierFuncFactoryContext func_context_;

  // Timer to manage table store compaction.
  px::event::TimerUPtr tablestore_compaction_timer_;

  px::metrics::MemoryMetrics memory_metrics_;
  // Timer to collect MemoryMetrics for this agent.
  px::event::TimerUPtr memory_metrics_timer_;

  // NATS connector for publishing to the metrics topic.
  std::unique_ptr<px::event::NATSConnector<messages::MetricsMessage>> metrics_nats_connector_;
  // Timer for pushing metrics onto NATS.
  px::event::TimerUPtr metrics_push_timer_;
};

/**
 * MessageHandler is the interface for all NATs based message handlers.
 * This interface can be registered with the manager and will be automatically
 * invoked based on the message type.
 *
 */
class Manager::MessageHandler {
 public:
  // Force initialization by subclasses.
  MessageHandler() = delete;

  /**
   * MessageHandler handles agent messages asynchronously and may respond over the
   * provided nats connection. agent_info and nats_conn lifetime must exceed that this object.
   */
  MessageHandler(px::event::Dispatcher* dispatcher, Info* agent_info,
                 Manager::VizierNATSConnector* nats_conn);

  virtual ~MessageHandler() = default;

  /**
   * Handle a message of the registered type. This function is called using the event loop thread.
   * Do not call blocking operators while handling the message.
   */
  virtual Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) = 0;

 protected:
  const Info* agent_info() const { return agent_info_; }
  Manager::VizierNATSConnector* nats_conn() { return nats_conn_; }
  px::event::Dispatcher* dispatcher() { return dispatcher_; }

 private:
  const Info* agent_info_;
  Manager::VizierNATSConnector* nats_conn_;

  px::event::Dispatcher* dispatcher_ = nullptr;
};

}  // namespace agent
}  // namespace vizier
}  // namespace px
