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

#include <map>
#include <string>
#include <vector>

#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"
#include "src/stirling/utils/utils.h"

namespace px::stirling::protocols::tls {
// Forward declaration so enum_range can be specialized.
enum class LegacyVersion : uint16_t;

}  // namespace px::stirling::protocols::tls

template <>
struct magic_enum::customize::enum_range<px::stirling::protocols::tls::LegacyVersion> {
  static constexpr int min = 0x0300;
  static constexpr int max = 0x0304;
};

namespace px {
namespace stirling {
namespace protocols {
namespace tls {

using ::px::utils::ToJSONString;

enum class ContentType : uint8_t {
  kChangeCipherSpec = 0x14,
  kAlert = 0x15,
  kHandshake = 0x16,
  kApplicationData = 0x17,
  kHeartbeat = 0x18,
};

enum class LegacyVersion : uint16_t {
  kSSL3 = 0x0300,
  kTLS1_0 = 0x0301,
  kTLS1_1 = 0x0302,
  kTLS1_2 = 0x0303,
  kTLS1_3 = 0x0304,
};

enum class AlertLevel : uint8_t {
  kWarning = 1,
  kFatal = 2,
};

enum class AlertDesc : uint8_t {
  kCloseNotify = 0,
  kUnexpectedMessage = 10,
  kBadRecordMAC = 20,
  kDecryptionFailed = 21,
  kRecordOverflow = 22,
  kDecompressionFailure = 30,
  kHandshakeFailure = 40,
  kNoCertificate = 41,
  kBadCertificate = 42,
  kUnsupportedCertificate = 43,
  kCertificateRevoked = 44,
  kCertificateExpired = 45,
  kCertificateUnknown = 46,
  kIllegalParameter = 47,
  kUnknownCA = 48,
  kAccessDenied = 49,
  kDecodeError = 50,
  kDecryptError = 51,
  kExportRestriction = 60,
  kProtocolVersion = 70,
  kInsufficientSecurity = 71,
  kInternalError = 80,
  kInappropriateFallback = 86,
  kUserCanceled = 90,
  kNoRenegotiation = 100,
  kUnsupportedExtension = 110,
  kUnrecognizedName = 112,
  kBadCertificateStatusResponse = 113,
  kBadCertificateHashValue = 114,
  kUnknownPSKIdentity = 115,
  kCertificateRequired = 116,
  kNoApplicationProtocol = 120,
  // TODO(ddelnano): Find a better way to represent this.
  kNoApplicationProtocol2 = 255,
};

enum class HandshakeType : uint8_t {
  kHelloRequest = 0,
  kClientHello = 1,
  kServerHello = 2,
  kNewSessionTicket = 4,
  kEncryptedExtensions = 8,  // TLS 1.3 only
  kCertificate = 11,
  kServerKeyExchange = 12,
  kCertificateRequest = 13,
  kServerHelloDone = 14,
  kCertificateVerify = 15,
  kClientKeyExchange = 16,
  kFinished = 20,
};

// Defined from
// https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#tls-extensiontype-values-1
enum class ExtensionType : uint16_t {
  kServerName = 0,
  kMaxFragmentLength = 1,
  kClientCertificateURL = 2,
  kTrustedCAKeys = 3,
  kTruncatedHMAC = 4,
  kStatusRequest = 5,
  kUserMapping = 6,
  kClientAuthz = 7,
  kServerAuthz = 8,
  kCertType = 9,
  kSupportedGroups = 10,
  kECPointFormats = 11,
  kSRP = 12,
  kSignatureAlgorithms = 13,
  kUseSRTP = 14,
  kHeartbeat = 15,
  kALPN = 16,
  kStatusRequestV2 = 17,
  kSignedCertificateTimestamp = 18,
  kClientCertificateType = 19,
  kServerCertificateType = 20,
  kPadding = 21,
  kEncryptThenMAC = 22,
  kExtendedMasterSecret = 23,
  kTokenBinding = 24,
  kCachedInfo = 25,
  kTLSLTS = 26,
  kCompressCertificate = 27,
  kRecordSizeLimit = 28,
  kPwdProtect = 29,
  kPwdClear = 30,
  kPasswordSalt = 31,
  kTicketPinning = 32,
  kTLSCertWithExternalPSK = 33,
  kDelegatedCredential = 34,
  kSessionTicket = 35,
  kTLMSP = 36,
  kTLMSPProxy = 37,
  kTLMSDelegate = 38,
  kSupportedEktCiphers = 39,
  kPreSharedKey = 41,
  kEarlyData = 42,
  kSupportedVersions = 43,
  kCookie = 44,
  kPSKKeyExchangeModes = 45,
  kCertificateAuthorities = 47,
  kOIDFilters = 48,
  kPostHandshakeAuth = 49,
  kSignatureAlgorithmsCert = 50,
  kKeyShare = 51,
  kTransparencyInfo = 52,
  kConnectionIdDeprecated = 53,
  kConnectionId = 54,
  kExternalIdHash = 55,
  kExternalSessionId = 56,
  kQuicTransportParameters = 57,
  kTicketRequest = 58,
  kDNSSecChain = 59,
  kSequenceNumberEncryptionAlgorithms = 60,
  kRRC = 61,
  kECHOuterExtensions = 64768,
  kEncryptedClientHello = 65037,
  kRenegotiationInfo = 65281,
};

struct Frame : public FrameBase {
  ContentType content_type;

  LegacyVersion legacy_version;

  uint16_t length = 0;

  HandshakeType handshake_type;

  uint24_t handshake_length = uint24_t(0);

  LegacyVersion handshake_version;

  std::string session_id;
  std::map<std::string, std::string> extensions;

  bool consumed = false;

  size_t ByteSize() const override { return sizeof(Frame); }

  std::string ToString() const override {
    return absl::Substitute(
        "TLS Frame [len=$0 content_type=$1 legacy_version=$2 handshake_version=$3 "
        "handshake_type=$4 extensions=$5]",
        length, content_type, legacy_version, handshake_version, handshake_type,
        ToJSONString(extensions));
  }
};

struct Record {
  Frame req;
  Frame resp;

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

using stream_id_t = uint16_t;
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Frame;
  using record_type = Record;
  using state_type = NoState;
  using key_type = stream_id_t;
};

}  // namespace tls
}  // namespace protocols
}  // namespace stirling
}  // namespace px
