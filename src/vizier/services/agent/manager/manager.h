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
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/metadata.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/manager/chan_cache.h"
#include "src/vizier/services/agent/manager/relation_info_manager.h"

#include "src/vizier/services/metadata/metadatapb/service.grpc.pb.h"

namespace pl {
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

/**
 * Info tracks basic information about and agent such as:
 * id, asid, hostname.
 */
struct Info {
  Info() = default;
  // Identification information for the agent.
  sole::uuid agent_id;
  // Agent short Id.
  uint32_t asid = 0;
  std::string hostname;
  std::string address;
  std::string pod_name;
  std::string host_ip;
  services::shared::agent::AgentCapabilities capabilities;
};

// Generates a service bearer token for authenticated requests.
std::string GenerateServiceToken();

// Adds service token to a GPRC context for authentication.
void AddServiceTokenToClientContext(grpc::ClientContext* ctx);

/**
 * Manager is the shared code and common interface for the entity responsible for managing the
 * sub-components of a pixie agent. The base version has a table store, carnot and metadata system.
 * This version can be extended to add more sub-components.
 */
class Manager : public pl::NotCopyable {
 public:
  using VizierNATSTLSConfig = pl::event::NATSTLSConfig;
  using VizierNATSConnector = pl::event::NATSConnector<pl::vizier::messages::VizierMessage>;
  using MsgCase = messages::VizierMessage::MsgCase;
  using MDSService = services::metadata::MetadataService;
  using MDSServiceSPtr = std::shared_ptr<Manager::MDSService::Stub>;
  using MDTPService = services::metadata::MetadataTracepointService;
  using MDTPServiceSPtr = std::shared_ptr<Manager::MDTPService::Stub>;

  Manager() = delete;
  virtual ~Manager() = default;

  // Forward decleration to prevent circular dependency on MessageHandler.
  class MessageHandler;

  /**
   * Run the main event loop. This function blocks and uses the thread to run the event loop.
   * The agent manager will continue to execute until Stop is called.
   */
  Status Run();

  /**
   * Stops the agent manager.
   * Safe to call from any thread.
   * \note Do not call this function from the destructor.
   */
  Status Stop(std::chrono::milliseconds timeout);

  /**
   * This function is called after registration of the agent is complete.
   * It's invoked on the event thread.
   */
  const Info* info() const { return &info_; }

  const carnot::Carnot* carnot() const { return carnot_.get(); }
  Status PostRegisterHook(uint32_t asid);
  Status PostReregisterHook(uint32_t asid);

 protected:
  // Protect constructor since we need to use Init on this class.
  Manager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
          int grpc_server_port, services::shared::agent::AgentCapabilities capabilities,
          std::string_view nats_url, std::string_view mds_url);
  Manager(sole::uuid agent_id, std::string_view pod_name, std::string_view host_ip,
          int grpc_server_port, services::shared::agent::AgentCapabilities capabilities,
          std::string_view mds_url, std::unique_ptr<VizierNATSConnector> nats_connector);
  Status Init();

  void NATSMessageHandler(VizierNATSConnector::MsgType msg);
  Status RegisterMessageHandler(MsgCase c, std::shared_ptr<MessageHandler> handler,
                                bool override = false);
  Status RegisterBackgroundHelpers();

  // ************************************************************
  // Static utility functions.
  // ************************************************************
  static std::unique_ptr<VizierNATSConnector> CreateDefaultNATSConnector(const sole::uuid& agent_id,
                                                                         std::string_view nats_url);

  static MDSServiceSPtr CreateDefaultMDSStub(
      std::string_view mds_addr, std::shared_ptr<grpc::ChannelCredentials> channel_creds);

  static MDTPServiceSPtr CreateDefaultMDTPStub(
      std::string_view mds_addr, std::shared_ptr<grpc::ChannelCredentials> channel_creds);

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
  pl::md::AgentMetadataStateManager* mds_manager() { return mds_manager_.get(); }
  RelationInfoManager* relation_info_manager() { return relation_info_manager_.get(); }
  pl::event::TimeSystem* time_system() { return time_system_.get(); }
  pl::event::Dispatcher* dispatcher() { return dispatcher_.get(); }

  Info* info() { return &info_; }
  VizierNATSConnector* nats_connector() { return nats_connector_.get(); }

 protected:
  std::shared_ptr<grpc::ChannelCredentials> grpc_channel_creds_;

  // The time system to use (real or simulated).
  std::unique_ptr<pl::event::TimeSystem> time_system_;
  pl::event::APIUPtr api_;
  pl::event::DispatcherUPtr dispatcher_;

  Info info_;
  std::unique_ptr<VizierNATSConnector> nats_connector_;

  // The controller is still running. Force stopping will cause un-graceful termination.
  std::atomic<bool> running_ = false;

  // The base agent contains the following components.
  std::shared_ptr<table_store::TableStore> table_store_;
  std::unique_ptr<carnot::Carnot> carnot_;
  std::unique_ptr<pl::md::AgentMetadataStateManager> mds_manager_;
  std::unique_ptr<RelationInfoManager> relation_info_manager_;

  // Factory context for vizier functions.
  funcs::VizierFuncFactoryContext func_context_;

 private:
  // Message handlers are registered per type of Vizier message.
  // same message handler can be used for multiple different types of messages.
  absl::flat_hash_map<MsgCase, std::shared_ptr<MessageHandler>> message_handlers_;
  void HandleMessage(std::unique_ptr<messages::VizierMessage> msg);

  // The timer to manage metadata updates.
  pl::event::TimerUPtr metadata_update_timer_;

  bool stop_called_ = false;

  // The data structure recording the metadata stored on this agent.
  std::unique_ptr<md::AgentMetadataFilter> agent_metadata_filter_;
  // Chan caches active connections to other Agents. Methods are all threadsafe.
  std::unique_ptr<ChanCache> chan_cache_;
  // The timer that runs the garbage collection routine.
  pl::event::TimerUPtr chan_cache_garbage_collect_timer_;
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
  MessageHandler(pl::event::Dispatcher* dispatcher, Info* agent_info,
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
  pl::event::Dispatcher* dispatcher() { return dispatcher_; }

 private:
  const Info* agent_info_;
  Manager::VizierNATSConnector* nats_conn_;

  pl::event::Dispatcher* dispatcher_ = nullptr;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
