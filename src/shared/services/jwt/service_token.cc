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

#include "src/shared/services/jwt/service_token.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <jwt/jwt.hpp>
#include <sole.hpp>

namespace px {
namespace services {

namespace {

constexpr std::string_view kBearerPrefix = "bearer ";

// How far in the past a freshly minted token's `nbf` is set, so that a verifier
// whose clock runs slightly behind the minter still accepts it.
constexpr std::chrono::seconds kNotBeforeSkew{60};

// cpp-jwt's built-in audience check reads `aud` as a string only, and throws a
// JSON type error on the array form. RFC 7519 4.1.3 permits both, so do the
// check here instead of passing jwt::params::aud().
//
// In practice both Pixie minting paths emit the string form -- the Go side sets
// WithFlattenAudience(true) explicitly for backwards compatibility -- but a
// token from any other RFC-conforming minter should still verify.
bool AudienceMatches(const ::jwt::jwt_payload& payload, const std::string& want) {
  if (!payload.has_claim("aud")) {
    return false;
  }
  try {
    return payload.get_claim_value<std::string>("aud") == want;
  } catch (const std::exception&) {
    // Not a string; fall through and try the array form.
  }
  try {
    const auto auds = payload.get_claim_value<std::vector<std::string>>("aud");
    return std::find(auds.begin(), auds.end(), want) != auds.end();
  } catch (const std::exception&) {
    return false;
  }
}

// Scopes is a comma-joined string rather than a JSON array, matching
// ProtoToToken in src/shared/services/utils/jwt.go.
Status CheckScopes(const ::jwt::jwt_payload& payload,
                   const std::vector<std::string>& required_scopes) {
  if (required_scopes.empty()) {
    return Status::OK();
  }
  if (!payload.has_claim("Scopes")) {
    return error::Unauthenticated("missing Scopes claim");
  }
  std::string scopes;
  try {
    scopes = payload.get_claim_value<std::string>("Scopes");
  } catch (const std::exception&) {
    return error::Unauthenticated("Scopes claim is not a string");
  }
  const std::vector<std::string_view> have = absl::StrSplit(scopes, ',');
  for (const auto& want : required_scopes) {
    if (std::find(have.begin(), have.end(), want) == have.end()) {
      return error::Unauthenticated("token lacks the '$0' scope", want);
    }
  }
  return Status::OK();
}

}  // namespace

Status VerifyServiceJWT(std::string_view token, std::string_view signing_key,
                        const ServiceJWTOptions& opts) {
  if (signing_key.empty()) {
    return error::InvalidArgument("signing key is empty");
  }
  if (token.empty()) {
    return error::Unauthenticated("empty token");
  }

  ::jwt::jwt_object obj;
  try {
    std::error_code ec;
    // decode() performs, in order: header parse, algorithm allowlist check
    // (which is what rejects `alg: none` and every algorithm other than
    // HS256), payload parse, the registered-claim checks below, and finally
    // HMAC signature verification. A clear `ec` means all of them passed, so
    // the claims read afterwards are signature-covered.
    obj =
        ::jwt::decode(std::string{token}, ::jwt::params::algorithms({"HS256"}), ec,
                      ::jwt::params::secret(std::string{signing_key}), ::jwt::params::verify(true),
                      ::jwt::params::leeway(static_cast<uint32_t>(opts.leeway.count())),
                      ::jwt::params::issuer(opts.issuer));
    if (ec) {
      return error::Unauthenticated("$0", ec.message());
    }
  } catch (const std::exception& e) {
    // cpp-jwt reports most failures through `ec`, but a few paths (memory
    // allocation, malformed base64 in some versions) still throw. A throw
    // escaping into a gRPC handler would terminate the process.
    return error::Unauthenticated("malformed token ($0)", e.what());
  }

  const auto& payload = obj.payload();

  // cpp-jwt validates `exp` only when the claim is present, so a token with no
  // expiry would otherwise verify and never expire.
  if (!payload.has_claim("exp")) {
    return error::Unauthenticated("missing exp claim");
  }
  if (!AudienceMatches(payload, opts.audience)) {
    return error::Unauthenticated("wrong audience (expected '$0')", opts.audience);
  }
  PX_RETURN_IF_ERROR(CheckScopes(payload, opts.required_scopes));

  // Deliberately no `sub` assertion: Pixie service tokens set sub to the
  // serviceID, which differs per caller. Authorization comes from Scopes.
  return Status::OK();
}

StatusOr<std::string> GenerateServiceToken(std::string_view signing_key,
                                           std::string_view service_id,
                                           std::chrono::seconds valid_for) {
  if (signing_key.empty()) {
    return error::InvalidArgument("cannot mint a service token with an empty signing key");
  }
  try {
    const auto now = std::chrono::system_clock::now();
    ::jwt::jwt_object obj{::jwt::params::algorithm("HS256")};
    obj.add_claim("iss", std::string{kPixieIssuer});
    obj.add_claim("aud", std::string{kVizierAudience});
    obj.add_claim("jti", sole::uuid4().str());
    obj.add_claim("iat", now);
    obj.add_claim("nbf", now - kNotBeforeSkew);
    obj.add_claim("exp", now + valid_for);
    obj.add_claim("sub", std::string{kServiceScope});
    obj.add_claim("Scopes", std::string{kServiceScope});
    obj.add_claim("ServiceID", std::string{service_id});
    obj.secret(std::string{signing_key});
    return obj.signature();
  } catch (const std::exception& e) {
    return error::Internal("failed to mint service token: $0", e.what());
  }
}

std::string_view StripBearerPrefix(std::string_view authorization_value) {
  if (!absl::StartsWithIgnoreCase(authorization_value, kBearerPrefix)) {
    return {};
  }
  return authorization_value.substr(kBearerPrefix.size());
}

}  // namespace services
}  // namespace px
