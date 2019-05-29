#pragma once

#include "third_party/foreign_cc/natsc/include/nats/nats.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "src/common/base/base.h"

PL_SUPPRESS_WARNINGS_START()
// TODO(michelle): Fix this so that we don't need to the NOLINT.
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace nats {

/**
 * NATS connector for a single topic.
 * @tparam TMsg message type. Must be a protobuf.
 */
template <typename TMsg>
class NATSConnector {
 public:
  NATSConnector(std::string_view nats_server, std::string_view pub_topic,
                std::string_view sub_topic)
      : nats_server_(std::string(nats_server)),
        pub_topic_(std::string(pub_topic)),
        sub_topic_(std::string(sub_topic)) {}
  virtual ~NATSConnector() {}

  /**
   * Connect to the nats server.
   * @return Status of the connection.
   */
  virtual Status Connect() {
    auto nats_status = natsConnection_ConnectTo(&nats_connection_, nats_server_.c_str());
    if (nats_status != NATS_OK) {
      return error::Unknown("Failed to connect to NATS, nats_status=$0", nats_status);
    }

    // Attach the message reader.
    natsConnection_Subscribe(&nats_subscription_, nats_connection_, sub_topic_.c_str(),
                             NATSMessageCallbackHandler, this);
    return Status::OK();
  }

  /**
   * Handle incoming message from NATS. This function is used by the C callback function. It can
   * also be used by Fakes/tests to inject new messages.
   * @param msg The natsMessage.
   */
  void NATSMessageHandler(natsConnection* /*nc*/, natsSubscription* /*sub*/, natsMsg* msg) {
    int len = natsMsg_GetDataLength(msg);
    const char* data = natsMsg_GetData(msg);
    auto parsed_msg = std::make_unique<TMsg>();

    bool ok = parsed_msg->ParseFromArray(data, len);
    if (!ok) {
      LOG(ERROR) << "Failed to parse message";
      return;
    }

    incoming_message_queue_.enqueue(std::move(parsed_msg));
  }

  /**
   * Tries to get a message with the specified timeout. If not available, nullptr is returned.
   * @param timeout the duration to wait.
   * @return unique_ptr to msg (or nullptr).
   */
  std::unique_ptr<TMsg> GetNextMessage(std::chrono::duration<int64_t> timeout) {
    std::unique_ptr<TMsg> msg;
    incoming_message_queue_.wait_dequeue_timed(msg, timeout);
    return msg;
  }

  size_t ApproximatePendingMessages() { return incoming_message_queue_.size_approx(); }

  /**
   * Publish a message to the NATS topic.
   * @param msg The protobuf message.
   * @return Status of publication.
   */
  virtual Status Publish(const TMsg& msg) {
    if (!nats_connection_) {
      return error::ResourceUnavailable("Not connected to NATS");
    }
    auto serialized_msg = msg.SerializeAsString();
    auto nats_status = natsConnection_Publish(nats_connection_, pub_topic_.c_str(),
                                              serialized_msg.c_str(), serialized_msg.size());
    if (nats_status != NATS_OK) {
      return error::Unknown("Failed to publish to NATS, nats_status=$0", nats_status);
    }
    return Status::OK();
  }

 protected:
  static void NATSMessageCallbackHandler(natsConnection* nc, natsSubscription* sub, natsMsg* msg,
                                         void* closure) {
    // We know that closure is of type NATSConnector.
    auto* connector = static_cast<NATSConnector<TMsg>*>(closure);
    connector->NATSMessageHandler(nc, sub, msg);
  }

  natsConnection* nats_connection_ = nullptr;
  natsSubscription* nats_subscription_ = nullptr;

  std::string nats_server_;
  std::string pub_topic_;
  std::string sub_topic_;
  moodycamel::BlockingConcurrentQueue<std::unique_ptr<TMsg>> incoming_message_queue_;
};

}  // namespace nats
}  // namespace pl
