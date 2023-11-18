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

#include <iomanip>
#include <map>
#include <string_view>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/parse.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mqtt/types.h"
#include "src/stirling/utils/binary_decoder.h"
#include "src/stirling/utils/parse_state.h"

#define PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(expr, val_or) \
  PX_ASSIGN_OR(expr, val_or, return ParseState::kNeedsMoreData)
#define PX_ASSIGN_OR_RETURN_INVALID(expr, val_or) \
  PX_ASSIGN_OR(expr, val_or, return ParseState::kInvalid)

namespace px {
namespace stirling {
namespace protocols {

namespace mqtt {

enum class MqttControlPacketType : uint8_t {
  CONNECT = 1,
  CONNACK = 2,
  PUBLISH = 3,
  PUBACK = 4,
  PUBREC = 5,
  PUBREL = 6,
  PUBCOMP = 7,
  SUBSCRIBE = 8,
  SUBACK = 9,
  UNSUBSCRIBE = 10,
  UNSUBACK = 11,
  PINGREQ = 12,
  PINGRESP = 13,
  DISCONNECT = 14,
  AUTH = 15
};

enum class PropertyCode : uint8_t {
  PayloadFormatIndicator = 0x01,
  MessageExpiryInterval = 0x02,
  ContentType = 0x03,
  ResponseTopic = 0x08,
  CorrelationData = 0x09,
  SubscriptionIdentifier = 0x0B,
  SessionExpiryInterval = 0x11,
  AssignedClientIdentifier = 0x12,
  ServerKeepAlive = 0x13,
  AuthenticationMethod = 0x15,
  AuthenticationData = 0x16,
  RequestProblemInformation = 0x17,
  WillDelayInterval = 0x18,
  RequestResponseInformation = 0x19,
  ResponseInformation = 0x1A,
  ServerReference = 0x1C,
  ReasonString = 0x1F,
  ReceiveMaximum = 0x21,
  TopicAliasMaximum = 0x22,
  TopicAlias = 0x23,
  MaximumQos = 0x24,
  RetainAvailable = 0x25,
  UserProperty = 0x26,
  MaximumPacketSize = 0x27,
  WildcardSubscriptionAvailable = 0x28,
  SubscriptionIdentifiersAvailable = 0x29,
  SharedSubscriptionAvailable = 0x2A
};

constexpr int kMaxVarInt8 = 128;
constexpr int kMaxVarInt16 = 16384;
constexpr int kMaxVarInt24 = 2097152;
constexpr int kMaxVarInt32 = 268435456;

static inline StatusOr<size_t> VariableEncodingNumBytes(uint64_t integer) {
  if (integer >= kMaxVarInt32) {
    return error::ResourceUnavailable("Maximum number of bytes exceeded for variable encoding.");
  }

  if (integer < kMaxVarInt8) {
    return 1;
  } else if (integer < kMaxVarInt16) {
    return 2;
  } else if (integer < kMaxVarInt24) {
    return 3;
  }
  return 4;
}

ParseState ParseProperties(Message* result, BinaryDecoder* decoder, size_t* properties_length) {
  uint8_t property_code;
  while (*properties_length > 0) {
    // Extracting the property code
    PX_ASSIGN_OR_RETURN_INVALID(property_code, decoder->ExtractBEInt<uint8_t>());
    *properties_length -= 1;

    auto property = magic_enum::enum_cast<PropertyCode>(property_code);
    if (!property.has_value()) {
      return ParseState::kInvalid;
    }

    switch (property.value()) {
      case PropertyCode::PayloadFormatIndicator: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t payload_format_indicator,
                                    decoder->ExtractBEInt<uint8_t>());
        if (payload_format_indicator == 0x00) {
          result->properties["payload_format"] = "unspecified";
        } else if (payload_format_indicator == 0x01) {
          result->properties["payload_format"] = "utf-8";
        } else {
          return ParseState::kInvalid;
        }
        *properties_length -= 1;
        break;
      }
      case PropertyCode::MessageExpiryInterval: {
        PX_ASSIGN_OR_RETURN_INVALID(uint32_t message_expiry_interval,
                                    decoder->ExtractBEInt<uint32_t>());
        result->properties["message_expiry_interval"] = std::to_string(message_expiry_interval);
        *properties_length -= 4;
        break;
      }
      case PropertyCode::ContentType: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view content_type,
                                    decoder->ExtractString(property_length));
        result->properties["content_type"] = std::string(content_type);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::ResponseTopic: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view response_topic,
                                    decoder->ExtractString(property_length));
        result->properties["response_topic"] = std::string(response_topic);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::CorrelationData: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view correlation_data,
                                    decoder->ExtractString(property_length));
        result->properties["correlation_data"] =
            BytesToString<bytes_format::HexAsciiMix>(correlation_data);

        *properties_length -= property_length;
        break;
      }
      case PropertyCode::SubscriptionIdentifier: {
        PX_ASSIGN_OR_RETURN_INVALID(uint64_t subscription_id, decoder->ExtractUVarInt());
        StatusOr<size_t> num_bytes_status = VariableEncodingNumBytes(subscription_id);
        if (!num_bytes_status.ok()) {
          return ParseState::kInvalid;
        }
        size_t num_bytes = num_bytes_status.ValueOrDie();

        result->properties["subscription_id"] = std::to_string(subscription_id);
        *properties_length -= num_bytes;
        break;
      }
      case PropertyCode::SessionExpiryInterval: {
        PX_ASSIGN_OR_RETURN_INVALID(uint32_t session_expiry_interval,
                                    decoder->ExtractBEInt<uint32_t>());
        result->properties["session_expiry_interval"] = std::to_string(session_expiry_interval);
        *properties_length -= 4;
        break;
      }
      case PropertyCode::AssignedClientIdentifier: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view assigned_client_identifier,
                                    decoder->ExtractString(property_length));
        result->properties["assigned_client_identifier"] = std::string(assigned_client_identifier);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::ServerKeepAlive: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t server_keep_alive, decoder->ExtractBEInt<uint16_t>());
        result->properties["server_keep_alive"] = std::to_string(server_keep_alive);
        *properties_length -= 2;
        break;
      }
      case PropertyCode::AuthenticationMethod: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view auth_method,
                                    decoder->ExtractString(property_length));
        result->properties["auth_method"] = std::string(auth_method);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::AuthenticationData: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view auth_data,
                                    decoder->ExtractString(property_length));
        result->properties["auth_data"] = BytesToString<bytes_format::HexAsciiMix>(auth_data);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::RequestProblemInformation: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t request_problem_information,
                                    decoder->ExtractBEInt<uint8_t>());
        result->properties["request_problem_information"] =
            std::to_string(request_problem_information);
        *properties_length -= 1;
        break;
      }
      case PropertyCode::WillDelayInterval: {
        PX_ASSIGN_OR_RETURN_INVALID(uint32_t will_delay_interval,
                                    decoder->ExtractBEInt<uint32_t>());
        result->properties["will_delay_interval"] = std::to_string(will_delay_interval);
        *properties_length -= 4;
        break;
      }
      case PropertyCode::RequestResponseInformation: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t request_response_information,
                                    decoder->ExtractBEInt<uint8_t>());
        result->properties["request_response_information"] =
            std::to_string(request_response_information);
        *properties_length -= 1;
        break;
      }
      case PropertyCode::ResponseInformation: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view response_information,
                                    decoder->ExtractString(property_length));
        result->properties["response_information"] = std::string(response_information);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::ServerReference: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view server_reference,
                                    decoder->ExtractString(property_length));
        result->properties["server_reference"] = std::string(server_reference);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::ReasonString: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t property_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view reason_string,
                                    decoder->ExtractString(property_length));
        result->properties["reason_string"] = std::string(reason_string);
        *properties_length -= property_length;
        break;
      }
      case PropertyCode::ReceiveMaximum: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t receive_maximum, decoder->ExtractBEInt<uint16_t>());
        result->properties["receive_maximum"] = std::to_string(receive_maximum);
        *properties_length -= 2;
        break;
      }
      case PropertyCode::TopicAliasMaximum: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t topic_alias_maximum,
                                    decoder->ExtractBEInt<uint16_t>());
        result->properties["topic_alias_maximum"] = std::to_string(topic_alias_maximum);
        *properties_length -= 2;
        break;
      }
      case PropertyCode::TopicAlias: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t topic_alias, decoder->ExtractBEInt<uint16_t>());
        result->properties["topic_alias"] = std::to_string(topic_alias);
        *properties_length -= 2;
        break;
      }
      case PropertyCode::MaximumQos: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t maximum_qos, decoder->ExtractBEInt<uint8_t>());
        result->properties["maximum_qos"] = std::to_string(maximum_qos);
        *properties_length -= 1;
        break;
      }
      case PropertyCode::RetainAvailable: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t retain_available, decoder->ExtractBEInt<uint8_t>());
        result->properties["retain_available"] = (retain_available == 1) ? "true" : "false";
        *properties_length -= 1;
        break;
      }
      case PropertyCode::UserProperty: {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t key_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view key, decoder->ExtractString(key_length));
        *properties_length -= key_length;
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t value_length, decoder->ExtractBEInt<uint16_t>());
        *properties_length -= 2;
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view value, decoder->ExtractString(value_length));
        *properties_length -= value_length;
        // For multiple user properties present, append to string if user property already present
        if (result->properties.find("user-properties") == result->properties.end()) {
          result->properties["user-properties"] =
              "{" + std::string(key) + ":" + std::string(value) + "}";
        } else {
          result->properties["user-properties"] +=
              ", {" + std::string(key) + ":" + std::string(value) + "}";
        }
        break;
      }
      case PropertyCode::MaximumPacketSize: {
        PX_ASSIGN_OR_RETURN_INVALID(uint32_t maximum_packet_size,
                                    decoder->ExtractBEInt<uint32_t>());
        result->properties["maximum_packet_size"] = std::to_string(maximum_packet_size);
        *properties_length -= 4;
        break;
      }
      case PropertyCode::WildcardSubscriptionAvailable: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t wildcard_subscription_available,
                                    decoder->ExtractBEInt<uint8_t>());
        result->properties["wildcard_subscription_available"] =
            (wildcard_subscription_available == 1) ? "true" : "false";
        *properties_length -= 1;
        break;
      }
      case PropertyCode::SubscriptionIdentifiersAvailable: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t subscription_id_available,
                                    decoder->ExtractBEInt<uint8_t>());
        result->properties["subscription_id_available"] =
            (subscription_id_available == 1) ? "true" : "false";
        *properties_length -= 1;
        break;
      }
      case PropertyCode::SharedSubscriptionAvailable: {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t shared_subscription_available,
                                    decoder->ExtractBEInt<uint8_t>());
        result->properties["shared_subscription_available"] =
            (shared_subscription_available == 1) ? "true" : "false";
        *properties_length -= 1;
        break;
      }
      default:
        return ParseState::kInvalid;
    }
  }
  return ParseState::kSuccess;
}

ParseState ParseVariableHeader(Message* result, BinaryDecoder* decoder,
                               const MqttControlPacketType& control_packet_type) {
  switch (control_packet_type) {
    case MqttControlPacketType::CONNECT: {
      PX_ASSIGN_OR_RETURN_INVALID(uint16_t protocol_name_length, decoder->ExtractBEInt<uint16_t>());
      PX_ASSIGN_OR_RETURN_INVALID(std::string_view protocol_name,
                                  decoder->ExtractString(protocol_name_length));
      CTX_DCHECK(protocol_name == "MQTT");
      PX_ASSIGN_OR_RETURN_INVALID(uint8_t protocol_version, decoder->ExtractBEInt<uint8_t>());
      CTX_DCHECK(protocol_version == 5);

      PX_ASSIGN_OR_RETURN_INVALID(uint8_t connect_flags, decoder->ExtractBEInt<uint8_t>());
      result->header_fields["username_flag"] = connect_flags >> 7;
      result->header_fields["password_flag"] = (connect_flags >> 6) & 0x1;
      result->header_fields["will_retain"] = (connect_flags >> 5) & 0x1;
      result->header_fields["will_qos"] = (connect_flags >> 3) & 0x3;
      result->header_fields["will_flag"] = (connect_flags >> 2) & 0x1;
      result->header_fields["clean_start"] = (connect_flags >> 1) & 0x1;

      PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["keep_alive"],
                                  decoder->ExtractBEInt<uint16_t>());

      PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
      if (!VariableEncodingNumBytes(properties_length).ok()) {
        return ParseState::kInvalid;
      }

      return ParseProperties(result, decoder, &properties_length);
    }
    case MqttControlPacketType::CONNACK: {
      PX_ASSIGN_OR_RETURN_INVALID(uint8_t connack_flags, decoder->ExtractBEInt<uint8_t>());
      PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["reason_code"],
                                  decoder->ExtractBEInt<uint8_t>());

      result->header_fields["session_present"] = connack_flags;

      PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
      if (!VariableEncodingNumBytes(properties_length).ok()) {
        return ParseState::kInvalid;
      }

      return ParseProperties(result, decoder, &properties_length);
    }
    case MqttControlPacketType::PUBLISH: {
      PX_ASSIGN_OR_RETURN_INVALID(uint16_t topic_length, decoder->ExtractBEInt<uint16_t>());
      PX_ASSIGN_OR_RETURN_INVALID(std::string_view topic_name,
                                  decoder->ExtractString(topic_length));
      result->payload["topic_name"] = std::string(topic_name);

      // Storing variable header length for use in payload length calculation
      result->header_fields["variable_header_length"] = 2 + (uint32_t)topic_length;

      // Check if packet qos is not 0, only then load packet id
      if (result->header_fields.find("qos") == result->header_fields.end()) {
        return ParseState::kInvalid;
      }
      if (result->header_fields["qos"] != 0) {
        PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["packet_identifier"],
                                    decoder->ExtractBEInt<uint16_t>());
        result->header_fields["variable_header_length"] += 2;
      }

      PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
      StatusOr<size_t> num_bytes_status = VariableEncodingNumBytes(properties_length);
      if (!num_bytes_status.ok()) {
        return ParseState::kInvalid;
      }
      size_t num_bytes = num_bytes_status.ValueOrDie();

      result->header_fields["variable_header_length"] += (uint32_t)(num_bytes + properties_length);

      return ParseProperties(result, decoder, &properties_length);
    }
    case MqttControlPacketType::PUBACK:
    case MqttControlPacketType::PUBREC:
    case MqttControlPacketType::PUBREL:
    case MqttControlPacketType::PUBCOMP: {
      PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["packet_identifier"],
                                  decoder->ExtractBEInt<uint16_t>());
      if (result->header_fields.find("remaining_length") == result->header_fields.end()) {
        return ParseState::kInvalid;
      }
      if (result->header_fields["remaining_length"] >= 3) {
        PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["reason_code"],
                                    decoder->ExtractBEInt<uint8_t>());
      }

      if (result->header_fields["remaining_length"] >= 4) {
        PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
        if (!VariableEncodingNumBytes(properties_length).ok()) {
          return ParseState::kInvalid;
        }
        return ParseProperties(result, decoder, &properties_length);
      }

      return ParseState::kSuccess;
    }
    case MqttControlPacketType::SUBSCRIBE:
    case MqttControlPacketType::SUBACK:
    case MqttControlPacketType::UNSUBSCRIBE:
    case MqttControlPacketType::UNSUBACK: {
      PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["packet_identifier"],
                                  decoder->ExtractBEInt<uint16_t>());
      // Storing variable header length for use in payload length calculation
      result->header_fields["variable_header_length"] = 2;
      PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
      StatusOr<size_t> num_bytes_status = VariableEncodingNumBytes(properties_length);
      if (!num_bytes_status.ok()) {
        return ParseState::kInvalid;
      }
      size_t num_bytes = num_bytes_status.ValueOrDie();

      result->header_fields["variable_header_length"] += num_bytes + properties_length;
      return ParseProperties(result, decoder, &properties_length);
    }
    case MqttControlPacketType::DISCONNECT: {
      PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["reason_code"],
                                  decoder->ExtractBEInt<uint8_t>());

      if (result->header_fields["remaining_length"] > 1) {
        PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
        if (!VariableEncodingNumBytes(properties_length).ok()) {
          return ParseState::kInvalid;
        }

        return ParseProperties(result, decoder, &properties_length);
      }
      return ParseState::kSuccess;
    }
    case MqttControlPacketType::AUTH: {
      if (result->header_fields["remaining_length"] == 0) {
        result->header_fields["reason_code"] = 0x00;
        return ParseState::kSuccess;
      }
      PX_ASSIGN_OR_RETURN_INVALID(result->header_fields["reason_code"],
                                  decoder->ExtractBEInt<uint8_t>());

      PX_ASSIGN_OR_RETURN_INVALID(size_t properties_length, decoder->ExtractUVarInt());
      if (!VariableEncodingNumBytes(properties_length).ok()) {
        return ParseState::kInvalid;
      }

      return ParseProperties(result, decoder, &properties_length);
    }
    default:
      return ParseState::kSuccess;
  }
}

ParseState ParsePayload(Message* result, BinaryDecoder* decoder,
                        const MqttControlPacketType& control_packet_type) {
  switch (control_packet_type) {
    case MqttControlPacketType::CONNECT: {
      PX_ASSIGN_OR_RETURN_INVALID(uint16_t client_id_length, decoder->ExtractBEInt<uint16_t>());
      PX_ASSIGN_OR_RETURN_INVALID(std::string_view client_id,
                                  decoder->ExtractString(client_id_length));
      result->payload["client_id"] = std::string(client_id);

      if (result->header_fields["will_flag"]) {
        PX_ASSIGN_OR_RETURN_INVALID(size_t will_properties_length, decoder->ExtractUVarInt());
        if (!VariableEncodingNumBytes(will_properties_length).ok()) {
          return ParseState::kInvalid;
        }

        if (ParseProperties(result, decoder, &will_properties_length) == ParseState::kInvalid) {
          return ParseState::kInvalid;
        }

        PX_ASSIGN_OR_RETURN_INVALID(size_t will_topic_length, decoder->ExtractBEInt<uint16_t>());
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view will_topic,
                                    decoder->ExtractString(will_topic_length));
        result->payload["will_topic"] = std::string(will_topic);

        PX_ASSIGN_OR_RETURN_INVALID(size_t will_payload_length, decoder->ExtractBEInt<uint16_t>());
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view will_payload,
                                    decoder->ExtractString(will_payload_length));
        result->payload["will_payload"] = std::string(will_payload);
      }

      if (result->header_fields["username_flag"]) {
        PX_ASSIGN_OR_RETURN_INVALID(size_t username_length, decoder->ExtractBEInt<uint16_t>());
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view username,
                                    decoder->ExtractString(username_length));
        result->payload["username"] = std::string(username);
      }

      if (result->header_fields["password_flag"]) {
        PX_ASSIGN_OR_RETURN_INVALID(size_t password_length, decoder->ExtractBEInt<uint16_t>());
        PX_ASSIGN_OR_RETURN_INVALID(std::ignore, decoder->ExtractString(password_length));
      }

      return ParseState::kSuccess;
    }
    case MqttControlPacketType::CONNACK:
      return ParseState::kSuccess;
    case MqttControlPacketType::PUBLISH: {
      if ((result->header_fields.find("remaining_length") == result->header_fields.end()) ||
          (result->header_fields.find("variable_header_length") == result->header_fields.end())) {
        return ParseState::kInvalid;
      }
      size_t payload_length = result->header_fields["remaining_length"] -
                              result->header_fields["variable_header_length"];
      PX_ASSIGN_OR_RETURN_INVALID(std::string_view payload, decoder->ExtractString(payload_length));
      result->payload["publish_message"] = std::string(payload);
      return ParseState::kSuccess;
    }
    case MqttControlPacketType::PUBACK:
    case MqttControlPacketType::PUBREC:
    case MqttControlPacketType::PUBREL:
    case MqttControlPacketType::PUBCOMP:
      return ParseState::kSuccess;
    case MqttControlPacketType::SUBSCRIBE: {
      if ((result->header_fields.find("remaining_length") == result->header_fields.end()) ||
          (result->header_fields.find("variable_header_length") == result->header_fields.end())) {
        return ParseState::kInvalid;
      }

      result->payload["topic_filter"] = "";
      result->payload["subscription_options"] = "";
      size_t payload_length = result->header_fields["remaining_length"] -
                              result->header_fields["variable_header_length"];
      while (payload_length > 0) {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t topic_filter_length,
                                    decoder->ExtractBEInt<uint16_t>());
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view topic_filter,
                                    decoder->ExtractString(topic_filter_length));
        if (result->payload["topic_filter"].empty()) {
          result->payload["topic_filter"] += std::string(topic_filter);
        } else {
          result->payload["topic_filter"] += ", " + std::string(topic_filter);
        }
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t subscription_options, decoder->ExtractBEInt<uint8_t>());
        std::map<std::string, uint8_t> subscription_opts(
            {{"maximum_qos", subscription_options & 0x3},
             {"no_local", (subscription_options >> 2) & 0x1},
             {"retain_as_published", (subscription_options >> 3) & 0x1},
             {"retain_handling", (subscription_options >> 4) & 0x3}});
        result->payload["subscription_options"] += ToJSONString(subscription_opts);
        payload_length -= (3 + topic_filter_length);
      }
      return ParseState::kSuccess;
    }
    case MqttControlPacketType::UNSUBSCRIBE: {
      if ((result->header_fields.find("remaining_length") == result->header_fields.end()) ||
          (result->header_fields.find("variable_header_length") == result->header_fields.end())) {
        return ParseState::kInvalid;
      }

      result->payload["topic_filter"] = "";
      size_t payload_length = result->header_fields["remaining_length"] -
                              result->header_fields["variable_header_length"];
      while (payload_length > 0) {
        PX_ASSIGN_OR_RETURN_INVALID(uint16_t topic_filter_length,
                                    decoder->ExtractBEInt<uint16_t>());
        PX_ASSIGN_OR_RETURN_INVALID(std::string_view topic_filter,
                                    decoder->ExtractString(topic_filter_length));
        if (result->payload["topic_filter"].empty()) {
          result->payload["topic_filter"] += std::string(topic_filter);
        } else {
          result->payload["topic_filter"] += ", " + std::string(topic_filter);
        }
        payload_length -= (2 + topic_filter_length);
      }
      return ParseState::kSuccess;
    }
    case MqttControlPacketType::SUBACK:
    case MqttControlPacketType::UNSUBACK: {
      if ((result->header_fields.find("remaining_length") == result->header_fields.end()) ||
          (result->header_fields.find("variable_header_length") == result->header_fields.end())) {
        return ParseState::kInvalid;
      }

      result->payload["reason_code"] = "";
      size_t payload_length = result->header_fields["remaining_length"] -
                              result->header_fields["variable_header_length"];
      while (payload_length > 0) {
        PX_ASSIGN_OR_RETURN_INVALID(uint8_t reason_code, decoder->ExtractBEInt<uint8_t>());
        if (result->payload["reason_code"].empty()) {
          result->payload["reason_code"] += std::to_string(reason_code);
        } else {
          result->payload["reason_code"] += ", " + std::to_string(reason_code);
        }
        payload_length -= 1;
      }
      return ParseState::kSuccess;
    }
    case MqttControlPacketType::PINGREQ:
    case MqttControlPacketType::PINGRESP:
    case MqttControlPacketType::DISCONNECT:
    case MqttControlPacketType::AUTH:
      return ParseState::kSuccess;
    default:
      return ParseState::kInvalid;
  }
}

ParseState ParseFrame(message_type_t type, std::string_view* buf, Message* result) {
  CTX_DCHECK(type == message_type_t::kRequest || type == message_type_t::kResponse);
  if (buf->size() < 2) {
    return ParseState::kNeedsMoreData;
  }

  BinaryDecoder decoder(*buf);

  // Parsing the fixed header
  // Control Packet Type extracted from first four bits of the first byte
  PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(uint8_t control_packet_code_flags,
                                      decoder.ExtractBEInt<uint8_t>());
  uint8_t control_packet_code = control_packet_code_flags >> 4;
  uint8_t control_packet_flags = control_packet_code_flags & 0x0F;

  auto control_packet_type_enum_cast =
      magic_enum::enum_cast<MqttControlPacketType>(control_packet_code);
  if (!control_packet_type_enum_cast.has_value()) {
    return ParseState::kInvalid;
  }
  const MqttControlPacketType control_packet_type = control_packet_type_enum_cast.value();

  result->control_packet_type = control_packet_code;

  // Saving the flags if control packet type is PUBLISH
  if (control_packet_type == MqttControlPacketType::PUBLISH) {
    result->dup = (control_packet_flags >> 3) != 0;
    result->retain = (control_packet_flags & 0x1) != 0;
    result->header_fields["qos"] = (control_packet_flags >> 1) & 0x3;
  }

  // Decoding the variable encoding of remaining length field
  size_t remaining_length;
  if (control_packet_type == MqttControlPacketType::PINGREQ ||
      control_packet_type == MqttControlPacketType::PINGRESP) {
    PX_ASSIGN_OR_RETURN_INVALID(remaining_length, decoder.ExtractUVarInt());
    if (remaining_length > 0) {
      return ParseState::kInvalid;
    }
  }

  // Eliminating cases where kNeedsMoreData needs to be returned
  // If buffer size is less than 4, there are chances that the remaining length is not present in
  // its entirety
  if (decoder.BufSize() < 4) {
    // Checking if buffer is complete
    PX_ASSIGN_OR_RETURN_NEEDS_MORE_DATA(remaining_length, decoder.ExtractUVarInt());
    // if remaining length is greater than 3 (4 if remaining length is included), then incomplete
    // buffer, otherwise buffer is complete
    if (remaining_length > 3) {
      return ParseState::kNeedsMoreData;
    }
  } else {
    PX_ASSIGN_OR_RETURN_INVALID(remaining_length, decoder.ExtractUVarInt());
    if (!VariableEncodingNumBytes(remaining_length).ok()) {
      return ParseState::kInvalid;
    }
  }

  // Making sure buffer is complete according to remaining length
  if (decoder.BufSize() < remaining_length) {
    return ParseState::kNeedsMoreData;
  }

  result->header_fields["remaining_length"] = remaining_length;

  if (ParseVariableHeader(result, &decoder, control_packet_type) == ParseState::kInvalid) {
    return ParseState::kInvalid;
  }

  if (ParsePayload(result, &decoder, control_packet_type) == ParseState::kInvalid) {
    return ParseState::kInvalid;
  }

  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

}  // namespace mqtt

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mqtt::Message* result,
                      NoState* /*state*/) {
  return mqtt::ParseFrame(type, buf, result);
}

template <>
size_t FindFrameBoundary<mqtt::Message>(message_type_t /*type*/, std::string_view buf,
                                        size_t start_pos, NoState* /*state*/) {
  return start_pos + buf.length();
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
