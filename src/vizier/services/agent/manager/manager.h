#pragma once

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
#include "src/vizier/messages/messagespb/messages.pb.h"
#include "src/vizier/services/agent/manager/relation_info_manager.h"

namespace pl {
namespace vizier {
namespace agent {

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
};

/**
 * Manager is the shared code and common interface for the entity responsible for managing the
 * sub-components of a pixie agent. The base version has a table store, carnot and metadata system.
 * This version can be extented to add more sub-components.
 */
class Manager : public pl::NotCopyable {
 public:
  using VizierNATSTLSConfig = pl::event::NATSTLSConfig;
  using VizierNATSConnector = pl::event::NATSConnector<pl::vizier::messages::VizierMessage>;
  using MsgCase = messages::VizierMessage::MsgCase;

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
  virtual Status PostRegisterHook() { return Status::OK(); }

 protected:
  // Protect constructor since we need to use Init on this class.
  Manager(sole::uuid agent_id, std::unique_ptr<VizierNATSConnector> nats_connector);
  Status Init();

  void NATSMessageHandler(VizierNATSConnector::MsgType msg);
  Status RegisterAgent();
  Status RegisterMessageHandler(MsgCase c, std::shared_ptr<MessageHandler> handler,
                                bool override = false);
  Status RegisterBackgroundHelpers();

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

  // APIs for the derived classes to reference the state of the agent.
  table_store::TableStore* table_store() { return table_store_.get(); }
  pl::md::AgentMetadataStateManager* mds_manager() { return mds_manager_.get(); }
  RelationInfoManager* relation_info_manager() { return relation_info_manager_.get(); }

  pl::event::TimeSystem* time_system() { return time_system_.get(); }
  pl::event::Dispatcher* dispatcher() { return dispatcher_.get(); }

 private:
  Info info_;
  std::unique_ptr<VizierNATSConnector> nats_connector_;

  // The controller is still running. Force stopping will cause un-graceful termination.
  std::atomic<bool> running_ = false;

  // The base agent contains the following components.
  std::shared_ptr<table_store::TableStore> table_store_;
  std::unique_ptr<carnot::Carnot> carnot_;
  std::unique_ptr<pl::md::AgentMetadataStateManager> mds_manager_;
  std::unique_ptr<RelationInfoManager> relation_info_manager_;

  // Message handlers are registered per type of Vizier message.
  // same message handler can be used for multiple different types of messages.
  absl::flat_hash_map<MsgCase, std::shared_ptr<MessageHandler>> message_handlers_;

  // Timeout for registration ACK.
  static constexpr std::chrono::seconds kRegistrationPeriod{5};

  // The time system to use (real or simulated).
  std::unique_ptr<pl::event::TimeSystem> time_system_;
  pl::event::APIUPtr api_;
  pl::event::DispatcherUPtr dispatcher_;

  // Only accessed from the event loop. So they don't need to be guarded by a mutex.
  bool agent_registered_ = false;
  pl::event::TimerUPtr registration_timeout_;
  void HandleMessage(std::unique_ptr<messages::VizierMessage> msg);
  void HandleRegisterAgentResponse(std::unique_ptr<messages::VizierMessage> msg);

  // The timer to manage metadata updates.
  pl::event::TimerUPtr metadata_update_timer_;
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
