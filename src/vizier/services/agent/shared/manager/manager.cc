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

#include "src/vizier/services/agent/shared/manager/manager.h"

#include <limits.h>

#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <utility>

#include <prometheus/text_serializer.h>
#include <jwt/jwt.hpp>

#include "src/common/base/base.h"
#include "src/common/metrics/metrics.h"
#include "src/common/perf/perf.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/funcs/funcs.h"
#include "src/vizier/services/agent/shared/manager/chan_cache.h"
#include "src/vizier/services/agent/shared/manager/config_manager.h"
#include "src/vizier/services/agent/shared/manager/exec.h"
#include "src/vizier/services/agent/shared/manager/heartbeat.h"
#include "src/vizier/services/agent/shared/manager/k8s_update.h"
#include "src/vizier/services/agent/shared/manager/registration.h"
#include "src/vizier/services/agent/shared/manager/ssl.h"
#include "src/vizier/services/metadata/metadatapb/service.grpc.pb.h"

namespace {

px::StatusOr<std::string> GetHostname() {
  char hostname[HOST_NAME_MAX];
  int err = gethostname(hostname, sizeof(hostname));
  if (err != 0) {
    return px::error::Unknown("Failed to get hostname");
  }
  return std::string(hostname);
}

}  // namespace

DEFINE_string(jwt_signing_key, gflags::StringFromEnv("PL_JWT_SIGNING_KEY", ""),
              "The JWT signing key for outgoing requests");

DEFINE_string(vizier_id, gflags::StringFromEnv("PL_VIZIER_ID", ""), "The ID of the cluster.");

DEFINE_string(vizier_name, gflags::StringFromEnv("PL_VIZIER_NAME", ""),
              "The name of the cluster according to vizier.");

DEFINE_string(vizier_namespace, gflags::StringFromEnv("PL_POD_NAMESPACE", ""),
              "The namespace in which vizier is deployed.");

namespace px {
namespace vizier {
namespace agent {
using ::px::event::Dispatcher;

Manager::MDSServiceSPtr CreateMDSStub(const std::shared_ptr<grpc::Channel>& chan) {
  if (chan == nullptr) {
    return nullptr;
  }
  return std::make_shared<Manager::MDSService::Stub>(chan);
}

Manager::MDTPServiceSPtr CreateMDTPStub(const std::shared_ptr<grpc::Channel>& chan) {
  if (chan == nullptr) {
    return nullptr;
  }
  return std::make_shared<Manager::MDTPService::Stub>(chan);
}

std::shared_ptr<services::metadata::CronScriptStoreService::Stub> CreateCronScriptStub(
    const std::shared_ptr<grpc::Channel>& chan) {
  if (chan == nullptr) {
    return nullptr;
  }
  return std::make_shared<services::metadata::CronScriptStoreService::Stub>(chan);
}

Manager::Manager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
                 int grpc_server_port, services::shared::agent::AgentCapabilities capabilities,
                 services::shared::agent::AgentParameters parameters, std::string_view nats_url,
                 std::string_view mds_url, system::KernelVersion kernel_version)
    : grpc_channel_creds_(SSL::DefaultGRPCClientCreds()),
      time_system_(std::make_unique<px::event::RealTimeSystem>()),
      api_(std::make_unique<px::event::APIImpl>(time_system_.get())),
      dispatcher_(api_->AllocateDispatcher("manager")),
      nats_addr_(nats_url),
      table_store_(std::make_shared<table_store::TableStore>()),
      relation_info_manager_(std::make_unique<RelationInfoManager>()),
      mds_channel_(grpc::CreateChannel(std::string(mds_url), grpc_channel_creds_)),
      func_context_(this, CreateMDSStub(mds_channel_), CreateMDTPStub(mds_channel_),
                    CreateCronScriptStub(mds_channel_), table_store_,
                    [](grpc::ClientContext* ctx) { AddServiceTokenToClientContext(ctx); }),
      memory_metrics_(&GetMetricsRegistry(), "agent_id", agent_id.str()) {
  // Register Vizier specific and carnot builtin functions.
  auto func_registry = std::make_unique<px::carnot::udf::Registry>("vizier_func_registry");
  ::px::vizier::funcs::RegisterFuncsOrDie(func_context_, func_registry.get());

  auto clients_config =
      std::make_unique<px::carnot::Carnot::ClientsConfig>(px::carnot::Carnot::ClientsConfig{
          [this](const std::string& address, const std::string& ssl_targetname) {
            return ResultSinkStubGenerator(address, ssl_targetname);
          },
          [](grpc::ClientContext* ctx) { AddServiceTokenToClientContext(ctx); }});
  auto server_config = std::make_unique<px::carnot::Carnot::ServerConfig>();
  server_config->grpc_server_creds = SSL::DefaultGRPCServerCreds();
  server_config->grpc_server_port = grpc_server_port;

  carnot_ = px::carnot::Carnot::Create(agent_id, std::move(func_registry), table_store_,
                                       std::move(clients_config), std::move(server_config))
                .ConsumeValueOrDie();

  info_.agent_id = agent_id;
  info_.capabilities = std::move(capabilities);
  info_.parameters = std::move(parameters);
  info_.pod_name = std::string(pod_name);
  info_.host_ip = std::string(host_ip);
  info_.kernel_version = kernel_version;
}

Status Manager::Init() {
  PX_ASSIGN_OR_RETURN(
      agent_metadata_filter_,
      md::AgentMetadataFilter::Create(kMetadataFilterMaxEntries, kMetadataFilterMaxErrorRate,
                                      md::kMetadataFilterEntities));
  chan_cache_ = std::make_unique<ChanCache>(kChanIdleGracePeriod);
  auto hostname_or_s = GetHostname();
  if (!hostname_or_s.ok()) {
    return hostname_or_s.status();
  }

  info_.hostname = hostname_or_s.ConsumeValueOrDie();
  info_.pid = getpid();

  LOG(INFO) << "Hostname: " << info_.hostname;

  // Set up the agent NATS connector.
  agent_nats_connector_ = std::make_unique<Manager::VizierNATSConnector>(
      nats_addr_, kAgentPubTopic /*pub_topic*/,
      absl::Substitute(kAgentSubTopicPattern, info_.agent_id.str()) /*sub topic*/,
      SSL::DefaultNATSCreds());

  // The first step is to connect to stats and register the agent.
  // Downstream dependencies like stirling/carnot depend on knowing
  // ASID and metadata state, which is only available after registration is
  // complete.
  PX_RETURN_IF_ERROR(agent_nats_connector_->Connect(dispatcher_.get()));
  // Attach the message handler for agent nats:
  agent_nats_connector_->RegisterMessageHandler(
      std::bind(&Manager::NATSMessageHandler, this, std::placeholders::_1));

  registration_handler_ = std::make_shared<RegistrationHandler>(
      dispatcher_.get(), &info_, agent_nats_connector_.get(),
      std::bind(&Manager::PostRegisterHook, this, std::placeholders::_1),
      std::bind(&Manager::PostReregisterHook, this, std::placeholders::_1));

  PX_CHECK_OK(RegisterMessageHandler(messages::VizierMessage::MsgCase::kRegisterAgentResponse,
                                     registration_handler_));
  registration_handler_->RegisterAgent();

  return InitImpl();
}

Status Manager::Run() {
  running_ = true;
  dispatcher_->Run(px::event::Dispatcher::RunType::Block);
  running_ = false;
  return Status::OK();
}

Status Manager::Stop(std::chrono::milliseconds timeout) {
  // Already stopping, protect against multiple calls.
  if (stop_called_) {
    return Status::OK();
  }
  stop_called_ = true;

  dispatcher_->Stop();
  auto s = StopImpl(timeout);

  // Wait for a limited amount of time for main thread to stop processing.
  std::chrono::time_point expiration_time = time_system_->MonotonicTime() + timeout;
  while (running_ && time_system_->MonotonicTime() < expiration_time) {
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }

  return s;
}

Status Manager::RegisterBackgroundHelpers() {
  metadata_update_timer_ = dispatcher_->CreateTimer([this]() {
    VLOG(1) << "State Update";
    ECHECK_OK(mds_manager_->PerformMetadataStateUpdate());
    if (metadata_update_timer_) {
      metadata_update_timer_->EnableTimer(std::chrono::seconds(5));
    }
  });
  metadata_update_timer_->EnableTimer(std::chrono::seconds(5));

  chan_cache_garbage_collect_timer_ = dispatcher_->CreateTimer([this]() {
    VLOG(1) << "GRPC channel cache garbage collection";
    ECHECK_OK(chan_cache_->CleanupChans());
    if (metadata_update_timer_) {
      chan_cache_garbage_collect_timer_->EnableTimer(kChanCacheCleanupChansionPeriod);
    }
  });
  chan_cache_garbage_collect_timer_->EnableTimer(kChanCacheCleanupChansionPeriod);

  // Add Heartbeat and execute query handlers.
  heartbeat_handler_ = std::make_shared<HeartbeatMessageHandler>(
      dispatcher_.get(), mds_manager_.get(), relation_info_manager_.get(), &info_,
      agent_nats_connector_.get());

  auto heartbeat_nack_handler = std::make_shared<HeartbeatNackMessageHandler>(
      dispatcher_.get(), &info_, agent_nats_connector_.get(),
      std::bind(&Manager::ReregisterHook, this));

  PX_CHECK_OK(
      RegisterMessageHandler(messages::VizierMessage::MsgCase::kHeartbeatAck, heartbeat_handler_));
  PX_CHECK_OK(RegisterMessageHandler(messages::VizierMessage::MsgCase::kHeartbeatNack,
                                     heartbeat_nack_handler));

  // Attach message handler for config updates.
  auto config_manager =
      std::make_shared<ConfigManager>(dispatcher_.get(), &info_, agent_nats_connector_.get());
  PX_RETURN_IF_ERROR(RegisterMessageHandler(messages::VizierMessage::MsgCase::kConfigUpdateMessage,
                                            config_manager));

  return Status::OK();
}

Status Manager::RegisterMessageHandler(Manager::MsgCase c, std::shared_ptr<MessageHandler> handler,
                                       bool override) {
  if (message_handlers_.contains(c) && !override) {
    return error::AlreadyExists("message handler already exists for case: $0", c);
  }
  message_handlers_[c] = handler;
  return Status::OK();
}

void Manager::NATSMessageHandler(Manager::VizierNATSConnector::MsgType msg) {
  // NATS returns data to us in an arbritrary thread. We need to handle it in the event
  // loop thread so we post to the event loop.

  // This funny pointer stuff is required because we generate an std::function,
  // that requires a copy of the lambda. The release allows us to recapture the value
  // into another unique pointer.
  messages::VizierMessage* m = msg.release();
  dispatcher_->Post(
      [m, this]() mutable { HandleMessage(std::unique_ptr<messages::VizierMessage>(m)); });
}

void Manager::HandleMessage(std::unique_ptr<messages::VizierMessage> msg) {
  VLOG(1) << "Manager::Run::GotMessage " << msg->DebugString();

  auto c = msg->msg_case();
  auto it = message_handlers_.find(c);
  if (it != message_handlers_.end()) {
    ECHECK_OK(it->second->HandleMessage(std::move(msg)))
        << "message handler failed... for type: " << c
        << " ignoring. Message: " << msg->DebugString();
    // Handler found.
  } else {
    LOG(ERROR) << "Unhandled message type: " << c << " Message: " << msg->DebugString();
  }
}

Status Manager::PostRegisterHook(uint32_t asid) {
  LOG_IF(FATAL, info_.asid != 0) << "Attempted to register existing agent with new ASID";
  info_.asid = asid;

  mds_manager_ = std::make_unique<px::md::AgentMetadataStateManagerImpl>(
      info_.hostname, info_.asid, info_.pid, info_.pod_name, info_.agent_id,
      info_.capabilities.collects_data(), px::system::Config::GetInstance(),
      agent_metadata_filter_.get(), sole::rebuild(FLAGS_vizier_id), FLAGS_vizier_name,
      FLAGS_vizier_namespace, time_system_.get());
  // Register the Carnot callback for metadata.
  carnot_->RegisterAgentMetadataCallback(
      std::bind(&px::md::AgentMetadataStateManager::CurrentAgentMetadataState, mds_manager_.get()));

  // Call the derived class post-register hook.
  PX_CHECK_OK(PostRegisterHookImpl());
  PX_CHECK_OK(RegisterBackgroundHelpers());

  k8s_nats_connector_ = std::make_unique<Manager::VizierNATSConnector>(
      nats_addr_, kK8sPubTopic /*pub_topic*/,
      absl::Substitute(kK8sSubTopicPattern, k8s_update_selector()) /*sub topic*/,
      SSL::DefaultNATSCreds());

  PX_RETURN_IF_ERROR(k8s_nats_connector_->Connect(dispatcher_.get()));

  auto k8s_update_handler =
      std::make_shared<K8sUpdateHandler>(dispatcher_.get(), mds_manager_.get(), &info_,
                                         k8s_nats_connector_.get(), k8s_update_selector());

  PX_CHECK_OK(RegisterMessageHandler(messages::VizierMessage::MsgCase::kK8SMetadataMessage,
                                     k8s_update_handler));

  // Attach the message handler for k8s nats:
  k8s_nats_connector_->RegisterMessageHandler(
      std::bind(&Manager::NATSMessageHandler, this, std::placeholders::_1));

  // Create a NATS connector for the metrics topic.
  metrics_nats_connector_ = std::make_unique<px::event::NATSConnector<messages::MetricsMessage>>(
      nats_addr_, kMetricsPubTopic /*pub topic*/, "" /*sub topic*/, SSL::DefaultNATSCreds());

  PX_RETURN_IF_ERROR(metrics_nats_connector_->Connect(dispatcher_.get()));

  tablestore_compaction_timer_ = dispatcher()->CreateTimer([this]() {
    // TODO(james): when we change ExecState::exec_mem_pool to not return just the default pool, we
    // will need to figure out how to use the correct memory pool here, but for now we can just use
    // the default pool.
    auto status = table_store()->RunCompaction(arrow::default_memory_pool());
    LOG_IF(ERROR, !status.ok()) << status.msg();
    if (tablestore_compaction_timer_) {
      tablestore_compaction_timer_->EnableTimer(kTableStoreCompactionPeriod);
    }
  });
  tablestore_compaction_timer_->EnableTimer(kTableStoreCompactionPeriod);

  memory_metrics_timer_ = dispatcher()->CreateTimer([this]() {
    memory_metrics_.MeasureMemory();
    if (memory_metrics_timer_) {
      memory_metrics_timer_->EnableTimer(kMemoryMetricsCollectPeriod);
    }
  });
  memory_metrics_timer_->EnableTimer(kMemoryMetricsCollectPeriod);

  metrics_push_timer_ = dispatcher()->CreateTimer([this]() {
    if (!metrics_nats_connector_) {
      LOG(ERROR) << "No NATS connection to send metrics on. Disabling metrics push timer.";
      // Returning without calling EnableTimer to prevent future timer calls.
      return;
    }
    auto& registry = GetMetricsRegistry();
    auto metrics = registry.Collect();
    int64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    for (auto& family : metrics) {
      for (auto& metric : family.metric) {
        metric.timestamp_ms = timestamp_ms;
      }
    }
    auto metrics_text = prometheus::TextSerializer().Serialize(metrics);

    messages::MetricsMessage msg;
    msg.set_prom_metrics_text(metrics_text);
    msg.set_pod_name(info_.pod_name);

    auto s = metrics_nats_connector_->Publish(msg);
    LOG_IF(ERROR, !s.ok()) << s.msg();

    if (metrics_push_timer_) {
      metrics_push_timer_->EnableTimer(kMetricsPushPeriod);
    }
  });
  metrics_push_timer_->EnableTimer(kMetricsPushPeriod);

  return Status::OK();
}

Status Manager::ReregisterHook() {
  LOG_IF(FATAL, heartbeat_handler_ == nullptr) << "Heartbeat handler is not set up";
  heartbeat_handler_->DisableHeartbeats();
  registration_handler_->ReregisterAgent();
  return Status::OK();
}

Status Manager::PostReregisterHook(uint32_t asid) {
  LOG_IF(FATAL, heartbeat_handler_ == nullptr) << "Heartbeat handler is not set up";
  LOG_IF(FATAL, asid != info_.asid) << "Received conflicting ASID after reregistration";
  heartbeat_handler_->EnableHeartbeats();
  return Status::OK();
}

std::unique_ptr<Manager::ResultSinkStub> Manager::ResultSinkStubGenerator(
    const std::string& remote_addr, const std::string& ssl_targetname) {
  auto chan = chan_cache_->GetChan(remote_addr);
  if (chan != nullptr) {
    return px::carnotpb::ResultSinkService::NewStub(chan);
  }

  grpc::ChannelArguments args;
  if (ssl_targetname.size()) {
    args.SetSslTargetNameOverride(ssl_targetname);
  }
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, 100000);
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 100000);
  args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
  args.SetInt(GRPC_ARG_HTTP2_BDP_PROBE, 1);
  args.SetInt(GRPC_ARG_HTTP2_MIN_RECV_PING_INTERVAL_WITHOUT_DATA_MS, 50000);
  args.SetInt(GRPC_ARG_HTTP2_MIN_SENT_PING_INTERVAL_WITHOUT_DATA_MS, 100000);

  chan = grpc::CreateCustomChannel(remote_addr, grpc_channel_creds_, args);
  chan_cache_->Add(remote_addr, chan);
  return px::carnotpb::ResultSinkService::NewStub(chan);
}

Manager::MessageHandler::MessageHandler(Dispatcher* dispatcher, Info* agent_info,
                                        Manager::VizierNATSConnector* nats_conn)
    : agent_info_(agent_info), nats_conn_(nats_conn), dispatcher_(dispatcher) {}

std::string GenerateServiceToken() {
  jwt::jwt_object obj{jwt::params::algorithm("HS256")};
  obj.add_claim("iss", "PL");
  obj.add_claim("aud", "vizier");
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

void AddServiceTokenToClientContext(grpc::ClientContext* grpc_context) {
  std::string token = GenerateServiceToken();
  grpc_context->AddMetadata("authorization", absl::Substitute("bearer $0", token));
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
