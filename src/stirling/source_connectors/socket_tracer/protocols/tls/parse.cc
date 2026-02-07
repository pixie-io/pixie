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
#include "src/stirling/source_connectors/socket_tracer/protocols/tls/parse.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace tls {

using px::utils::JSONObjectBuilder;

constexpr size_t kTLSRecordHeaderLength = 5;
constexpr size_t kExtensionMinimumLength = 4;
constexpr size_t kSNIExtensionMinimumLength = 3;

// In TLS 1.3, Random is 32 bytes.
// In TLS 1.2 and earlier, gmt_unix_time is 4 bytes and Random is 28 bytes.
constexpr size_t kRandomStructLength = 32;

StatusOr<ParseState> ExtractSNIExtension(SharedExtensions* exts, BinaryDecoder* decoder) {
  PX_ASSIGN_OR(auto server_name_list_length, decoder->ExtractBEInt<uint16_t>(),
               return ParseState::kInvalid);
  while (server_name_list_length > 0) {
    PX_ASSIGN_OR(auto server_name_type, decoder->ExtractBEInt<uint8_t>(),
                 return error::Internal("Failed to extract server name type"));

    // This is the only valid value for server_name_type and corresponds to host_name.
    DCHECK_EQ(server_name_type, 0);

    PX_ASSIGN_OR(auto server_name_length, decoder->ExtractBEInt<uint16_t>(),
                 return error::Internal("Failed to extract server name length"));
    PX_ASSIGN_OR(auto server_name, decoder->ExtractString(server_name_length),
                 return error::Internal("Failed to extract server name"));

    exts->server_names.push_back(std::string(server_name));
    server_name_list_length -= kSNIExtensionMinimumLength + server_name_length;
  }
  return ParseState::kSuccess;
}

/*
 * The TLS wire protocol is best described in each of the RFCs for the protocol
 * SSL v3.0: https://tools.ietf.org/html/rfc6101
 * TLS v1.0: https://tools.ietf.org/html/rfc2246
 * TLS v1.1: https://tools.ietf.org/html/rfc4346
 * TLS v1.2: https://tools.ietf.org/html/rfc5246
 * TLS v1.3: https://tools.ietf.org/html/rfc8446
 *
 * These specs have c struct style definitions of the wire protocol. The wikipedia
 * page is also a good resource to see it explained in a more typical ascii binary format
 * diagram: https://en.wikipedia.org/wiki/Transport_Layer_Security#TLS_record
 */

ParseState ParseFullFrame(SharedExtensions* extensions, BinaryDecoder* decoder, Frame* frame) {
  PX_ASSIGN_OR(auto raw_content_type, decoder->ExtractBEInt<uint8_t>(),
               return ParseState::kInvalid);
  auto content_type = magic_enum::enum_cast<tls::ContentType>(raw_content_type);
  if (!content_type.has_value()) {
    return ParseState::kInvalid;
  }
  frame->content_type = content_type.value();

  PX_ASSIGN_OR(auto legacy_version, decoder->ExtractBEInt<uint16_t>(), return ParseState::kInvalid);
  auto lv = magic_enum::enum_cast<tls::LegacyVersion>(legacy_version);
  if (!lv.has_value()) {
    return ParseState::kInvalid;
  }
  frame->legacy_version = lv.value();

  PX_ASSIGN_OR(frame->length, decoder->ExtractBEInt<uint16_t>(), return ParseState::kInvalid);

  if (frame->content_type == tls::ContentType::kApplicationData ||
      frame->content_type == tls::ContentType::kChangeCipherSpec ||
      frame->content_type == tls::ContentType::kAlert ||
      frame->content_type == tls::ContentType::kHeartbeat) {
    if (!decoder->ExtractBufIgnore(frame->length).ok()) {
      return ParseState::kInvalid;
    }
    return ParseState::kSuccess;
  }

  PX_ASSIGN_OR(auto raw_handshake_type, decoder->ExtractBEInt<uint8_t>(),
               return ParseState::kInvalid);
  auto handshake_type = magic_enum::enum_cast<tls::HandshakeType>(raw_handshake_type);
  if (!handshake_type.has_value()) {
    return ParseState::kInvalid;
  }
  frame->handshake_type = handshake_type.value();

  PX_ASSIGN_OR(auto handshake_length, decoder->ExtractBEInt<uint24_t>(),
               return ParseState::kInvalid);
  frame->handshake_length = handshake_length;

  PX_ASSIGN_OR(auto raw_handshake_version, decoder->ExtractBEInt<uint16_t>(),
               return ParseState::kInvalid);
  auto handshake_version = magic_enum::enum_cast<tls::LegacyVersion>(raw_handshake_version);
  if (!handshake_version.has_value()) {
    return ParseState::kInvalid;
  }
  frame->handshake_version = handshake_version.value();

  // Skip the random struct.
  if (!decoder->ExtractBufIgnore(kRandomStructLength).ok()) {
    return ParseState::kInvalid;
  }

  PX_ASSIGN_OR(auto session_id_len, decoder->ExtractBEInt<uint8_t>(), return ParseState::kInvalid);
  if (session_id_len > 32) {
    return ParseState::kInvalid;
  }

  if (session_id_len > 0) {
    PX_ASSIGN_OR(frame->session_id, decoder->ExtractString(session_id_len),
                 return ParseState::kInvalid);
  }

  PX_ASSIGN_OR(auto cipher_suite_length, decoder->ExtractBEInt<uint16_t>(),
               return ParseState::kInvalid);
  if (frame->handshake_type == HandshakeType::kClientHello) {
    if (!decoder->ExtractBufIgnore(cipher_suite_length).ok()) {
      return ParseState::kInvalid;
    }
  }

  PX_ASSIGN_OR(auto compression_methods_length, decoder->ExtractBEInt<uint8_t>(),
               return ParseState::kInvalid);
  if (frame->handshake_type == HandshakeType::kClientHello) {
    if (!decoder->ExtractBufIgnore(compression_methods_length).ok()) {
      return ParseState::kInvalid;
    }
  }

  // TODO(ddelnano): Test TLS 1.2 and earlier where extensions are not present
  PX_ASSIGN_OR(auto extensions_length, decoder->ExtractBEInt<uint16_t>(),
               return ParseState::kInvalid);
  if (extensions_length == 0) {
    return ParseState::kSuccess;
  }

  while (extensions_length > 0) {
    PX_ASSIGN_OR(auto extension_type, decoder->ExtractBEInt<uint16_t>(),
                 return ParseState::kInvalid);
    PX_ASSIGN_OR(auto extension_length, decoder->ExtractBEInt<uint16_t>(),
                 return ParseState::kInvalid);

    if (extension_length > 0) {
      if (extension_type == 0x00) {
        if (!ExtractSNIExtension(extensions, decoder).ok()) {
          return ParseState::kInvalid;
        }
      } else {
        if (!decoder->ExtractBufIgnore(extension_length).ok()) {
          return ParseState::kInvalid;
        }
      }
    }

    extensions_length -= kExtensionMinimumLength + extension_length;
  }
  JSONObjectBuilder body_builder;
  body_builder.WriteKVRecursive("extensions", *extensions);
  frame->body = body_builder.GetString();

  return ParseState::kSuccess;
}

}  // namespace tls

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, tls::Frame* frame, NoState*) {
  // TLS record header is 5 bytes. The size of the record is in bytes 4 and 5.
  if (buf->length() < tls::kTLSRecordHeaderLength) {
    return ParseState::kNeedsMoreData;
  }
  uint16_t length = static_cast<uint8_t>((*buf)[3]) << 8 | static_cast<uint8_t>((*buf)[4]);
  if (buf->length() < length + tls::kTLSRecordHeaderLength) {
    return ParseState::kNeedsMoreData;
  }

  BinaryDecoder decoder(*buf);
  std::unique_ptr<tls::SharedExtensions> extensions;
  if (type == kRequest) {
    extensions = std::make_unique<tls::ReqExtensions>();
  } else {
    extensions = std::make_unique<tls::RespExtensions>();
  }
  auto parse_result = tls::ParseFullFrame(extensions.get(), &decoder, frame);
  if (parse_result == ParseState::kSuccess) {
    buf->remove_prefix(length + tls::kTLSRecordHeaderLength);
  }
  return parse_result;
}

template <>
size_t FindFrameBoundary<tls::Frame>(message_type_t, std::string_view, size_t, NoState*) {
  // Not implemented.
  return std::string::npos;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
