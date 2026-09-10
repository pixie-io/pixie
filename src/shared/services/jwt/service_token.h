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

// C++ counterpart to src/shared/services/utils/jwt.go. Mint and verify the
// HS256 service tokens Pixie components present to each other as
// `authorization: bearer <jwt>`.
//
// Mint and verify deliberately live in the same translation unit: they encode
// one claim contract (aud/iss/Scopes/exp) and silently drifting apart is the
// failure mode that matters. The Go side is the other implementation of the
// same contract -- keep the three in sync.
//
// All cryptography is delegated to cpp-jwt (HMAC + constant-time compare via
// BoringSSL). Do not hand-roll base64, HMAC, or signature comparison here.

#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace services {

// The claim values every in-cluster Pixie service token carries. These mirror
// the Go minting path in src/shared/services/utils/jwt.go.
inline constexpr std::string_view kVizierAudience = "vizier";
inline constexpr std::string_view kPixieIssuer = "PL";
inline constexpr std::string_view kServiceScope = "service";

// How long a freshly minted service token stays valid. Short by design: the
// verifier keeps no `jti` replay cache, so the validity window is what bounds
// the damage from a captured token.
inline constexpr std::chrono::seconds kDefaultServiceTokenValidity{60};

struct ServiceJWTOptions {
  // Required `aud` claim. RFC 7519 4.1.3 allows `aud` to be either a string or
  // an array of strings; both forms are accepted and this value need only
  // appear among them.
  std::string audience{kVizierAudience};

  // Required `iss` claim, compared exactly.
  std::string issuer{kPixieIssuer};

  // Scopes that must ALL appear in the comma-joined `Scopes` claim. Note that
  // Pixie service tokens put "service" in `Scopes` and the serviceID in `sub`
  // -- do not assert on `sub`, it varies per caller.
  std::vector<std::string> required_scopes{std::string{kServiceScope}};

  // Clock-skew tolerance applied to `exp` and `nbf`.
  std::chrono::seconds leeway{30};
};

// VerifyServiceJWT checks `token` against `signing_key` and returns OK only if
// it is a well-formed HS256 JWT with a valid signature, an unexpired `exp`
// (which must be present), and claims satisfying `opts`.
//
// The returned Status message names the specific failure and is intended for
// operator logs. Callers serving untrusted peers should NOT forward it -- see
// the collapsing in DirectQueryServer's authenticator.
//
// Rejects, among others: `alg: none` and every non-HS256 algorithm, tokens
// signed with the wrong key, expired tokens, and tokens whose header, payload,
// or signature has been tampered with.
Status VerifyServiceJWT(std::string_view token, std::string_view signing_key,
                        const ServiceJWTOptions& opts = {});

// GenerateServiceToken mints a token that VerifyServiceJWT accepts, with
// `service_id` recorded in the `ServiceID` claim.
//
// Returns an error rather than throwing on an empty signing key: cpp-jwt raises
// jwt::SigningError in that case, and an uncaught throw here would abort the
// process on its first outgoing service call.
StatusOr<std::string> GenerateServiceToken(
    std::string_view signing_key, std::string_view service_id,
    std::chrono::seconds valid_for = kDefaultServiceTokenValidity);

// StripBearerPrefix returns the token following a case-insensitive "bearer "
// prefix, or an empty view if the prefix is absent.
//
// gRPC lowercases metadata keys but not values. Pixie's own C++ and Go minting
// paths both emit a lowercase "bearer " prefix, while RFC 6750 spells it
// "Bearer"; accept either.
std::string_view StripBearerPrefix(std::string_view authorization_value);

}  // namespace services
}  // namespace px
