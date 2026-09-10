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

using ::jwt::decode;
using ::jwt::jwt_object;
using ::jwt::jwt_payload;
using ::jwt::params::algorithm;
using ::jwt::params::algorithms;
using ::jwt::params::issuer;
using ::jwt::params::leeway;
using ::jwt::params::secret;
using ::jwt::params::verify;

namespace {

constexpr std::string_view kBearerPrefix = "bearer ";
constexpr std::chrono::seconds kNotBeforeSkew{60};

constexpr std::chrono::seconds kTokenValidity{60};

// cpp-jwt's built-in audience check reads `aud` as a string only and throws on
// the array form, which RFC 7519 4.1.3 permits.
bool AudienceMatches(const jwt_payload& payload, const std::string& want) {
  const auto& claims = payload.create_json_obj();
  const auto it = claims.find("aud");
  if (it == claims.end()) {
    return false;
  }
  if (it->is_string()) {
    return it->get_ref<const std::string&>() == want;
  }
  if (it->is_array()) {
    for (const auto& entry : *it) {
      if (entry.is_string() && entry.get_ref<const std::string&>() == want) {
        return true;
      }
    }
  }
  return false;
}

Status CheckScopes(const jwt_payload& payload, const std::vector<std::string>& required_scopes) {
  if (required_scopes.empty()) {
    return Status::OK();
  }
  const auto& claims = payload.create_json_obj();
  const auto it = claims.find("Scopes");
  if (it == claims.end() || !it->is_string()) {
    return error::Unauthenticated("missing or non-string Scopes claim");
  }
  const std::vector<std::string_view> have = absl::StrSplit(it->get_ref<const std::string&>(), ',');
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

  std::error_code ec;
  jwt_object obj;
  try {
    obj = decode(std::string{token}, algorithms({"HS256"}), ec, secret(std::string{signing_key}),
                 verify(true), leeway(static_cast<uint32_t>(opts.leeway.count())),
                 issuer(opts.issuer));
  } catch (const std::exception& e) {
    return error::Unauthenticated("malformed token ($0)", e.what());
  }
  if (ec) {
    return error::Unauthenticated("$0", ec.message());
  }

  // A clear `ec` means the signature verified, so the claims below are covered
  // by it.
  const auto& payload = obj.payload();

  // cpp-jwt validates `exp` only when the claim is present, so a token minted
  // without one would otherwise verify forever.
  if (!payload.has_claim("exp")) {
    return error::Unauthenticated("missing exp claim");
  }
  if (!AudienceMatches(payload, opts.audience)) {
    return error::Unauthenticated("wrong audience (expected '$0')", opts.audience);
  }
  return CheckScopes(payload, opts.required_scopes);
}

StatusOr<std::string> GenerateServiceToken(std::string_view signing_key,
                                           std::string_view service_id) {
  if (signing_key.empty()) {
    return error::InvalidArgument("cannot mint a service token with an empty signing key");
  }
  const auto now = std::chrono::system_clock::now();
  jwt_object obj{algorithm("HS256")};
  obj.add_claim("iss", std::string{kPixieIssuer});
  obj.add_claim("aud", std::string{kVizierAudience});
  obj.add_claim("jti", sole::uuid4().str());
  obj.add_claim("iat", now);
  obj.add_claim("nbf", now - kNotBeforeSkew);
  obj.add_claim("exp", now + kTokenValidity);
  obj.add_claim("sub", std::string{kServiceScope});
  obj.add_claim("Scopes", std::string{kServiceScope});
  obj.add_claim("ServiceID", std::string{service_id});
  obj.secret(std::string{signing_key});

  std::error_code ec;
  auto token = obj.signature(ec);
  if (ec) {
    return error::Internal("failed to mint service token: $0", ec.message());
  }
  return token;
}

std::string_view StripBearerPrefix(std::string_view authorization_value) {
  if (!absl::StartsWithIgnoreCase(authorization_value, kBearerPrefix)) {
    return {};
  }
  return authorization_value.substr(kBearerPrefix.size());
}

}  // namespace services
}  // namespace px
