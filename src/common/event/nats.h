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

#include <nats/nats.h>
#include "src/common/event/libuv.h"

#include <memory>
#include <string>

namespace px {
namespace event {

struct NATSTLSConfig {
  std::string ca_cert;
  std::string tls_key;
  std::string tls_cert;
};

class NATSConnectorBase {
 protected:
  NATSConnectorBase(std::string_view nats_server, std::unique_ptr<NATSTLSConfig> tls_config)
      : nats_server_(std::string(nats_server)), tls_config_(std::move(tls_config)) {}

  Status ConnectBase(Dispatcher* base_dispatcher);

  natsConnection* nats_connection_ = nullptr;

  std::string nats_server_;
  std::unique_ptr<NATSTLSConfig> tls_config_;
  static void DisconnectedCB(natsConnection* nc, void* closure);
  static void ReconnectedCB(natsConnection* nc, void* closure);

 private:
  size_t disconnect_count_ = 0;
  size_t reconnect_count_ = 0;
};

/**
 * NATS connector for a single topic.
 * @tparam TMsg message type. Must be a protobuf.
 */
template <typename TMsg>
class NATSConnector : public NATSConnectorBase {
 public:
  using MsgType = std::unique_ptr<TMsg>;
  using MessageHandlerCB = std::function<void(MsgType)>;
  NATSConnector(std::string_view nats_server, std::string_view pub_topic,
                std::string_view sub_topic, std::unique_ptr<NATSTLSConfig> tls_config)
      : NATSConnectorBase(nats_server, std::move(tls_config)),
        pub_topic_(std::string(pub_topic)),
        sub_topic_(std::string(sub_topic)) {}

  virtual ~NATSConnector() {
    if (nats_connection_ != nullptr) {
      natsConnection_Destroy(nats_connection_);
    }

    if (nats_subscription_ != nullptr) {
      natsSubscription_Destroy(nats_subscription_);
    }
    nats_Close();
  }

  /**
   * Connect to the nats server.
   * @return Status of the connection.
   */
  virtual Status Connect(Dispatcher* base_dispatcher) {
    PX_RETURN_IF_ERROR(ConnectBase(base_dispatcher));
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

    if (msg_handler_) {
      msg_handler_(std::move(parsed_msg));
    } else {
      LOG(WARNING) << "Dropping message (no handler registered): " << parsed_msg->DebugString();
    }
  }

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
      nats_PrintLastErrorStack(stderr);
      return error::Unknown("Failed to publish to NATS, nats_status=$0", nats_status);
    }
    return Status::OK();
  }

  /**
   * Register the message handler.
   * The lifetime of the handler (and bound variables), must exceed the lifetime of this class,
   * or RemoveMessageHandler must be called.
   */
  void RegisterMessageHandler(MessageHandlerCB handler) { msg_handler_ = std::move(handler); }

  /**
   * Remove the message handler if attached.
   */
  void RemoveMessageHandler() { msg_handler_ = nullptr; }

 protected:
  static void NATSMessageCallbackHandler(natsConnection* nc, natsSubscription* sub, natsMsg* msg,
                                         void* closure) {
    // We know that closure is of type NATSConnector.
    auto* connector = static_cast<NATSConnector<TMsg>*>(closure);
    connector->NATSMessageHandler(nc, sub, msg);
    natsMsg_Destroy(msg);
  }

  natsSubscription* nats_subscription_ = nullptr;

  std::string pub_topic_;
  std::string sub_topic_;

  // The registered message handler.
  MessageHandlerCB msg_handler_;
};

}  // namespace event
}  // namespace px
